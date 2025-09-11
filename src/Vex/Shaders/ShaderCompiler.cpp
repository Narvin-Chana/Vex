#include "ShaderCompiler.h"

#include <string>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/Shaders/CompilerBase.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/TextureSampler.h>

namespace vex
{

ShaderCompiler::ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , compilerSettings(compilerSettings)
    , dxcCompilerImpl(compilerSettings.shaderIncludeDirectories)
#if VEX_SLANG
    , slangCompilerImpl(compilerSettings.shaderIncludeDirectories)
#endif
{
#if VEX_SHIPPING
    // Force disable shader debugging in shipping.
    this->compilerSettings.enableShaderDebugging = false;
#endif
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderEnvironment ShaderCompiler::CreateShaderEnvironment(ShaderCompilerBackend compiler)
{
    ShaderEnvironment env;
    env.defines.emplace_back("VEX_DEBUG", std::to_string(VEX_DEBUG));
    env.defines.emplace_back("VEX_DEVELOPMENT", std::to_string(VEX_DEVELOPMENT));
    env.defines.emplace_back("VEX_SHIPPING", std::to_string(VEX_SHIPPING));
    env.defines.emplace_back("VEX_RAYTRACING",
                             GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing) ? "1" : "0");

    rhi->ModifyShaderCompilerEnvironment(compiler, env);

    return env;
}

std::expected<void, std::string> ShaderCompiler::CompileShader(Shader& shader)
{
    if (!GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::BindlessResources))
    {
        VEX_LOG(Fatal, "Vex requires BindlessResources in order to bind global resources.");
    }

    ShaderCompilerBackend shaderCompilerToUse = ShaderCompilerBackend::Auto;

    std::expected<std::vector<byte>, std::string> res = std::unexpected("Invalid shader compiler backend.");
    // Identify which shader compiler to use.
    switch (shader.key.compiler)
    {
    case ShaderCompilerBackend::Auto:
#if VEX_SLANG
        // Default is DXC, unless VEX_SLANG and the shader file has .slang extension.
        if (shader.key.path.extension() == ".slang")
        {
            shaderCompilerToUse = ShaderCompilerBackend::Slang;
            break;
        }
        // Voluntary fallthrough:
#endif
    case ShaderCompilerBackend::DXC:
        shaderCompilerToUse = ShaderCompilerBackend::DXC;
        break;
#if VEX_SLANG
    case ShaderCompilerBackend::Slang:
        shaderCompilerToUse = ShaderCompilerBackend::Slang;
        break;
#endif
    }

    // Setup compilation environment.
    ShaderEnvironment env = CreateShaderEnvironment(shaderCompilerToUse);

    // Compile the shader.
    if (shaderCompilerToUse == ShaderCompilerBackend::DXC)
    {
        res = dxcCompilerImpl.CompileShader(shader, env, compilerSettings);
    }
#if VEX_SLANG
    else if (shaderCompilerToUse == ShaderCompilerBackend::Slang)
    {
        res = slangCompilerImpl.CompileShader(shader, env, compilerSettings);
    }
#endif

    if (!res.has_value())
    {
        return std::unexpected(res.error());
    }

    std::vector<byte>& shaderBytecode = res.value();

    // Outputs raw bytecode if we ever need to figure out a difficult shader issue (for spirv can use spirv-dis
    // to read raw .spv file, similar tools like RenderDoc work for dxil).
#define VEX_OUTPUT_BYTECODE 0
#if VEX_OUTPUT_BYTECODE
    std::filesystem::path outputPath = shader.key.path;
#if VEX_VULKAN
    outputPath.replace_extension(".spv"); // or ".spirv"
#elif VEX_DX12
    outputPath.replace_extension(".dxil");
#endif

    std::ofstream file(outputPath, std::ios::binary);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(shaderBytecode.data()), shaderBytecode.size());
        file.close();
        VEX_LOG(Info, "Shader bytecode written to: {}", outputPath.string());
    }
    else
    {
        VEX_LOG(Error, "Failed to write shader bytecode to: {}", outputPath.string());
    }
#endif // VEX_OUTPUT_BYTECODE

    // Store shader bytecode blob inside the Shader.
    shader.blob = std::move(shaderBytecode);

    shader.version++;
    shader.isDirty = false;

    return {};
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key)
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
        auto result = CompileShader(*shaderPtr);
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
    // For now we recompile ALL shaders upon request.
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