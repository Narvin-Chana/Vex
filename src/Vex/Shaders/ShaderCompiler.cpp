#include "ShaderCompiler.h"

#include <regex>
#include <string>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/Shaders/CompilerBase.h>
#include <Vex/Shaders/DXCImpl.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderResourceContext.h>
#include <Vex/Shaders/SlangImpl.h>
#include <Vex/TextureSampler.h>

namespace vex
{

ShaderCompiler::ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , compilerSettings(compilerSettings)
{
#if VEX_SHIPPING
    // Force disable shader debugging in shipping.
    this->compilerSettings.enableShaderDebugging = false;
#endif

    switch (compilerSettings.compilerBackend)
    {
    case ShaderCompilerBackend::DXC:
    {
        compilerImpl = MakeUnique<DXCCompilerImpl>(compilerSettings.shaderIncludeDirectories);
        break;
    }
#if VEX_SLANG
    case ShaderCompilerBackend::Slang:
    {
        compilerImpl = MakeUnique<SlangCompilerImpl>(compilerSettings.shaderIncludeDirectories);
        break;
    }
#endif
    default:
        VEX_LOG(Fatal, "Unsupported shader compilation backend!");
        break;
    }
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderEnvironment ShaderCompiler::CreateShaderEnvironment()
{
    ShaderEnvironment env;
    env.defines.emplace_back(L"VEX_DEBUG", std::to_wstring(VEX_DEBUG));
    env.defines.emplace_back(L"VEX_DEVELOPMENT", std::to_wstring(VEX_DEVELOPMENT));
    env.defines.emplace_back(L"VEX_SHIPPING", std::to_wstring(VEX_SHIPPING));
    env.defines.emplace_back(L"VEX_RAYTRACING",
                             GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing) ? L"1" : L"0");

    rhi->ModifyShaderCompilerEnvironment(compilerSettings.compilerBackend, env);

    return env;
}

std::expected<void, std::string> ShaderCompiler::CompileShader(Shader& shader,
                                                               const ShaderResourceContext& resourceContext)
{
    if (!GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::BindlessResources))
    {
        VEX_LOG(Fatal, "Vex requires BindlessResources in order to bind global resources.");
    }

    ShaderEnvironment env = CreateShaderEnvironment();
    auto res = compilerImpl->CompileShader(shader, env, compilerSettings, resourceContext);
    if (!res.has_value())
    {
        return std::unexpected(res.error());
    }

    std::vector<u8>& shaderBytecode = res.value();

    // Store shader bytecode blob inside the Shader.
    shader.blob = std::move(shaderBytecode);

    shader.version++;
    shader.isDirty = false;

    return {};
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext)
{
    Shader* shaderPtr;
    if (auto el = shaderCache.find(key); el != shaderCache.end())
    {
        shaderPtr = &el->second;
    }
    else
    {
        // Avoids the default constructor being called (it is not defined for class Shader)
        shaderCache.emplace(std::piecewise_construct, std::make_tuple(key), std::make_tuple(Shader(key)));
        shaderPtr = &shaderCache.find(key)->second;
    }

    if (shaderPtr->NeedsRecompile())
    {
        auto result = CompileShader(*shaderPtr, resourceContext);
        if (!result.has_value())
        {
            if (compilerSettings.enableShaderDebugging)
            {
                shaderPtr->isErrored = true;
                compilationErrors.emplace_back(key, result.error());
            }
            // If we're not in a debugShaders context, a non-compiling shader is fatal.
            VEX_LOG(compilerSettings.enableShaderDebugging ? Error : Fatal,
                    "Failed to compile shader:\n\t- {}:\n\t- Reason: {}",
                    key,
                    result.error());
        }
    }

    return NonNullPtr(shaderPtr);
}

std::pair<bool, std::size_t> ShaderCompiler::IsShaderStale(const Shader& shader) const
{
    // if (!std::filesystem::exists(shader.key.path))
    //{
    //     VEX_LOG(Fatal, "Unable to parse a shader file which no longer exists: {}.", shader.key);
    //     return { false, shader.hash };
    // }

    // std::optional<std::size_t> newHash = compilerImpl->GetShaderHash(shader);
    // if (!newHash)
    //{
    //     return { false, shader.hash };
    // }

    // bool isShaderStale = shader.hash != *newHash;
    // return { isShaderStale, *newHash };

    // TODO: figure out a better way to determine which shaders are due for recompilation...
    return { true, 0 };
}

void ShaderCompiler::MarkShaderDirty(const ShaderKey& key)
{
    auto shader = shaderCache.find(key);
    if (shader == shaderCache.end())
    {
        VEX_LOG(Error,
                "The shader key passed did not yield any valid shaders in the shader cache (key {}). "
                "Unable to mark it "
                "as dirty.",
                key);
        return;
    }

    shader->second.MarkDirty();
    shader->second.isErrored = false;
}

void ShaderCompiler::MarkAllShadersDirty()
{
    for (auto& [key, shader] : shaderCache)
    {
        shader.MarkDirty();
        shader.isErrored = false;
    }

    VEX_LOG(Info, "Marked all shaders for recompilation...");
}

void ShaderCompiler::MarkAllStaleShadersDirty()
{
    u32 numStaleShaders = 0;
    for (auto& [key, shader] : shaderCache)
    {
        // TODO: figure out a better way to determine which shaders are due for recompilation...
        // if (auto [isShaderStale, newShaderHash] = IsShaderStale(shader); isShaderStale || shader.isErrored)
        {
            // shader.hash = newShaderHash;
            shader.MarkDirty();
            shader.isErrored = false;
            numStaleShaders++;
        }
    }

    VEX_LOG(Info, "Marked {} shader(s) for recompilation...", numStaleShaders);
}

void ShaderCompiler::SetCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback)
{
    errorsCallback = callback;
}

void ShaderCompiler::FlushCompilationErrors()
{
    // If user requests a recompilation (depends on the callback), remove the isErrored flag from all
    // errored shaders.
    if (errorsCallback && errorsCallback(compilationErrors))
    {
        for (auto& [key, error] : compilationErrors)
        {
            auto el = shaderCache.find(key);
            VEX_ASSERT(el != shaderCache.end(), "A shader in compilationErrors was not found in the cache...");
            // The next time we attempt to use this shader, it will be recompiled.
            el->second.isErrored = false;
        }
        compilationErrors.clear();
    }
}

} // namespace vex