#include "ShaderCompiler.h"

#include <string>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Shaders/CompilerBase.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderEnvironment.h>

namespace vex
{

namespace ShaderUtil
{
bool IsBuiltInSemantic(std::string_view name)
{
    return name.substr(0, 3) == "SV_";
}
bool CanReflectShaderType(ShaderType type)
{
    switch (type)
    {
    case ShaderType::ComputeShader:
    case ShaderType::PixelShader:
    case ShaderType::VertexShader:
        return true;
    default:;
    }
    return false;
}
} // namespace ShaderUtil

ShaderCompiler::ShaderCompiler(const ShaderCompilerSettings& compilerSettings)
    : compilerSettings(compilerSettings)
    , dxcCompilerImpl(compilerSettings.shaderIncludeDirectories)
#if VEX_SLANG
    , slangCompilerImpl(compilerSettings.shaderIncludeDirectories)
#endif
{
#if VEX_SHIPPING
    // Force disable shader debugging in shipping.
    this->compilerSettings.enableShaderDebugging = false;
#endif
    globalShaderEnv = CreateShaderEnvironment();
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderEnvironment ShaderCompiler::CreateShaderEnvironment()
{
    ShaderEnvironment env;
    env.defines.emplace_back("VEX_DEBUG", std::to_string(VEX_DEBUG));
    env.defines.emplace_back("VEX_DEVELOPMENT", std::to_string(VEX_DEVELOPMENT));
    env.defines.emplace_back("VEX_SHIPPING", std::to_string(VEX_SHIPPING));
    env.defines.emplace_back("VEX_RAYTRACING", std::to_string(GPhysicalDevice->IsFeatureSupported(Feature::RayTracing)));
    env.defines.emplace_back("VEX_VULKAN", std::to_string(VEX_VULKAN));
    env.defines.emplace_back("VEX_DX12", std::to_string(VEX_DX12));
    return env;
}

std::expected<void, std::string> ShaderCompiler::CompileShader(Shader& shader)
{
    return GetCompiler(shader.key).and_then([&](CompilerBase* compiler) { return CompileShader(compiler, shader); });
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key)
{
    if (!key.path.empty() && !key.sourceCode.empty())
    {
        VEX_LOG(Warning,
                "Shader {} has both a shader filepath and shader source string. Using the filepath for compilation...",
                key);
    }

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

    if (shaderPtr->needsRecompile)
    {
        shaderPtr->needsRecompile = false;
        std::expected<void, std::string> res = GetCompiler(key).and_then(
            [&](CompilerBase* compiler)
            {
                return compiler->GetShaderCodeHash(*shaderPtr, globalShaderEnv, compilerSettings)
                    .and_then(
                        [&](const SHA1HashDigest& digest) -> std::expected<void, std::string>
                        {
                            if (digest != shaderPtr->res.sourceHash)
                            {
                                return CompileShader(compiler, *shaderPtr)
                                    .and_then(
                                        [&]() -> std::expected<void, std::string>
                                        {
                                            shaderPtr->res.sourceHash = digest;
                                            return {};
                                        });
                            }
                            return {};
                        });
            });

        if (!res.has_value())
        {
            if (compilerSettings.enableShaderDebugging)
            {
                shaderPtr->isErrored = true;
                compilationErrors.emplace_back(key, res.error());
            }
            // If we're not in a debugShaders context, a non-compiling shader is fatal.
            VEX_LOG(compilerSettings.enableShaderDebugging ? Error : Fatal,
                    "Failed to compile shader:\n\t- {}:\n\t- Reason: {}",
                    key,
                    res.error());
        }
    }

    return NonNullPtr(shaderPtr);
}

bool ShaderCompiler::IsShaderStale(const Shader& shader) const
{
    // TODO(https://trello.com/c/UquJz7ow): figure out a better way to determine which shaders are due for recompilation
    // For now all shaders are considered stale when asked
    return true;
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
    for (auto& [_, shader] : shaderCache)
    {
        if (auto isShaderStale = IsShaderStale(shader); isShaderStale || shader.isErrored)
        {
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

std::expected<void, std::string> ShaderCompiler::CompileShader(CompilerBase* compiler, Shader& shader)
{
    return compiler->CompileShader(shader, globalShaderEnv, compilerSettings)
        .and_then(
            [&](ShaderCompilationResult result) -> std::expected<void, std::string>
            {
                // Outputs raw bytecode if we ever need to figure out a difficult shader issue (for spirv can use
                // spirv-dis to read raw .spv file, similar tools like RenderDoc work for dxil).
                if (compilerSettings.dumpShaderOutputBytecode)
                {
                    const std::vector<byte>& shaderBytecode = result.compiledCode;

                    std::filesystem::path outputPath =
                        std::filesystem::current_path() / "VexOutput_SHADER_BYTECODE" / shader.key.path.filename();
#if VEX_VULKAN
                    outputPath.replace_extension(".spv"); // or ".spirv"
#elif VEX_DX12
                    outputPath.replace_extension(".dxil");
#endif
                    if (!std::filesystem::exists(outputPath.parent_path()))
                    {
                        std::filesystem::create_directories(outputPath.parent_path());
                    }
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
                }

                // Store shader bytecode blob inside the Shader.
                shader.res = std::move(result);
                shader.version++;

                return {};
            });
}

std::expected<NonNullPtr<CompilerBase>, std::string> ShaderCompiler::GetCompiler(const ShaderKey& key)
{
    switch (key.compiler)
    {
    case ShaderCompilerBackend::Auto:
#if VEX_SLANG
        // Default is DXC, unless VEX_SLANG and the shader file has .slang extension.
        if (key.path.extension() == ".slang")
            return NonNullPtr<CompilerBase>{ &slangCompilerImpl };
        // Intentionnal fallthrough...
#endif
    case ShaderCompilerBackend::DXC:
        return NonNullPtr<CompilerBase>{ &dxcCompilerImpl };
#if VEX_SLANG
    case ShaderCompilerBackend::Slang:
        return NonNullPtr<CompilerBase>{ &slangCompilerImpl };
#endif
    }
    return std::unexpected("Invalid shader compiler backend.");
}

} // namespace vex