#include "ShaderCompiler.h"

#include <ranges>
#include <string>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/RayTracing.h>
#include <Vex/Utility/Validation.h>

#include <ShaderCompiler/RayTracingShaderKey.h>
#include <ShaderCompiler/Shader.h>

#define VEX_HAS_AT_LEAST_ONE_COMPILER (VEX_DXC || VEX_SLANG)

namespace vex::sc
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

std::filesystem::path GetShaderDumpPath(const Shader& shader)
{
    static auto ShortenType = [](ShaderType type)
    {
        switch (type)
        {
        case ShaderType::VertexShader:
            return "VS";
        case ShaderType::PixelShader:
            return "PS";
        case ShaderType::ComputeShader:
            return "CS";
        case ShaderType::RayGenerationShader:
            return "RayGen";
        case ShaderType::RayMissShader:
            return "RayMiss";
        case ShaderType::RayClosestHitShader:
            return "ClosestHit";
        case ShaderType::RayAnyHitShader:
            return "AnyHit";
        case ShaderType::RayIntersectionShader:
            return "RayIntersect";
        case ShaderType::RayCallableShader:
            return "RayCallable";
        }
        return "";
    };

    const std::string shortShaderType = ShortenType(shader.GetKey().type);

    std::string uniqueFilename = std::format("{}_{}", shader.GetKey().entryPoint, ShortenType(shader.GetKey().type));
    // Ensure the filepath isn't too long (OS limit is usually ~250, but we also have to store the current_path()).
    static constexpr std::size_t MaxFilenameLength = 150;
    uniqueFilename.resize(std::min(uniqueFilename.size(), MaxFilenameLength));

    std::filesystem::path basePath =
        std::filesystem::current_path() / "VexOutput_SHADER_BYTECODE" /
        std::format("{}/{}", shader.GetKey().compiler, std::hash<ShaderKey>{}(shader.GetKey()));
    return basePath / uniqueFilename;
}

} // namespace ShaderUtil

ShaderCompiler::ShaderCompiler(const ShaderCompilerSettings& compilerSettings)
    : compilerSettings(compilerSettings)
#if VEX_DXC
    , dxcCompiler(compilerSettings.shaderIncludeDirectories)
#endif
#if VEX_SLANG
    , slangCompiler(compilerSettings.shaderIncludeDirectories)
#endif
{
    globalShaderEnv = CreateShaderEnvironment(compilerSettings);
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderView ShaderCompiler::GetShaderView(const ShaderKey& key)
{
    return ShaderView(*GetShader(key));
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key)
{
    return GetShader(key, true);
}

RayTracingShaderCollection ShaderCompiler::GetRayTracingShaderCollection(const RayTracingShaderKey& rtShaderKey)
{
    RayTracingShaderCollection shaderCollection{
        .maxRecursionDepth = rtShaderKey.maxRecursionDepth,
        .maxPayloadByteSize = rtShaderKey.maxPayloadByteSize,
        .maxAttributeByteSize = rtShaderKey.maxAttributeByteSize,
    };

    for (auto& rayGenKey : rtShaderKey.rayGenerationShaders)
    {
        shaderCollection.rayGenerationShaders.push_back(GetShaderView(rayGenKey));
    }
    for (auto& rayMissKey : rtShaderKey.rayMissShaders)
    {
        shaderCollection.rayMissShaders.push_back(GetShaderView(rayMissKey));
    }
    for (const auto& [name, rayClosestHitKey, rayAnyHitKey, rayIntersectionKey] : rtShaderKey.hitGroups)
    {
        shaderCollection.hitGroups.push_back({
            .name = name,
            .rayClosestHitShader = GetShaderView(rayClosestHitKey),
            .rayAnyHitShader = rayAnyHitKey ? std::optional{ GetShaderView(*rayAnyHitKey) } : std::nullopt,
            .rayIntersectionShader =
                rayIntersectionKey ? std::optional{ GetShaderView(*rayIntersectionKey) } : std::nullopt,
        });
    }
    for (auto& rayCallableKey : rtShaderKey.rayCallableShaders)
    {
        shaderCollection.rayCallableShaders.push_back(GetShaderView(rayCallableKey));
    }

    return shaderCollection;
}

std::optional<std::string> ShaderCompiler::CompileShaderFromFilepath(const ShaderKey& key)
{
    VEX_ASSERT(
        VEX_HAS_AT_LEAST_ONE_COMPILER,
        "Can only compile a shader from filepath if the shader compiler has atleast once valid compiler backend.");
    VEX_CHECK(!key.filepath.empty(),
              "Error compiling shader {} from filepath: Cannot compile from an empty filepath.",
              key);
    const std::optional<std::filesystem::path> filepathString = TryGetFilepathFromVirtualFilepath(key);
    VEX_CHECK(filepathString.has_value(), "Unable to find shader at filepath: {}", key.filepath);
    Shader& shader = *GetShader(key, false);
    CompilerBase& compiler = GetCompiler(key);
    return HandleCompiledShader(shader,
                                compiler.CompileShader(shader, *filepathString, globalShaderEnv, compilerSettings));
}

std::optional<std::string> ShaderCompiler::CompileShaderFromSourceCode(const ShaderKey& key,
                                                                       const std::string_view sourceCode)
{
    VEX_ASSERT(
        VEX_HAS_AT_LEAST_ONE_COMPILER,
        "Can only compile a shader from source code if the shader compiler has atleast once valid compiler backend.");
    VEX_CHECK(!key.filepath.empty(),
              "Error compiling shader {} from source string: You must provide a virtual filepath for the shader.",
              key);
    VEX_CHECK(!sourceCode.empty(), "Error compiling shader {} from source code: Cannot compile an empty shader.", key);
    Shader& shader = *GetShader(key, false);
    CompilerBase& compiler = GetCompiler(key);
    return HandleCompiledShader(shader, compiler.CompileShader(shader, sourceCode, globalShaderEnv, compilerSettings));
}

void ShaderCompiler::SetShaderCompilationErrorsCallback(ShaderHotReloadErrorsCallback&& callback)
{
    if (!compilerSettings.enableShaderHotReload)
    {
        VEX_LOG(
            Warning,
            "Setting the shader compilation errors callback when not in shader hot-reload mode will have no effect...");
        return;
    }
    errorsCallback = std::move(callback);
}

void ShaderCompiler::RecompileChangedShaders()
{
    std::vector<ShaderKey> changedShaders;
    for (auto& [key, shader] : shaderCache)
    {
        if (const bool isShaderStale = IsShaderStale(shader); !shader.IsValid() || isShaderStale)
        {
            changedShaders.emplace_back(key);
        }
    }
    const u32 numRecompiledShaders = HotReloadShaders(changedShaders);
    VEX_LOG(Info, "Recompiled all changed filepath-based shaders ({})...", numRecompiledShaders);
}

void ShaderCompiler::RecompileAllShaders()
{
    const u32 numRecompiledShaders = HotReloadShaders(shaderCache | std::views::keys);
    VEX_LOG(Info, "Recompiled all filepath-based shaders ({})...", numRecompiledShaders);
}

void ShaderCompiler::RecompileShaders(const Span<const ShaderKey> shaderKeys)
{
    const u32 numRecompiledShaders = HotReloadShaders(shaderKeys);
    VEX_LOG(Info, "Recompiled the passed-in filepath-based shaders ({})...", numRecompiledShaders);
}

std::optional<std::filesystem::path> ShaderCompiler::TryGetFilepathFromVirtualFilepath(const ShaderKey& key)
{
    std::filesystem::path filepath{ key.filepath };
    if (!std::filesystem::exists(filepath))
    {
        return std::nullopt;
    }
    return filepath;
}

ShaderEnvironment ShaderCompiler::CreateShaderEnvironment(const ShaderCompilerSettings& compilerSettings)
{
    ShaderEnvironment env;
    env.defines.emplace_back("VEX_SPIRV", std::to_string(compilerSettings.target == CompilationTarget::SPIRV));
    env.defines.emplace_back("VEX_DXIL", std::to_string(compilerSettings.target == CompilationTarget::DXIL));
    return env;
}

CompilerBase& ShaderCompiler::GetCompiler(const ShaderKey& key)
{
    const std::optional<std::filesystem::path> filepath = TryGetFilepathFromVirtualFilepath(key);
    const std::string extension = filepath.has_value() ? filepath->extension().string() : "NONE";
    switch (key.compiler)
    {
    case ShaderCompilerBackend::Auto:
#if VEX_SLANG
        // Default is DXC, unless VEX_SLANG and the shader file has .slang extension.
        if (extension == ".slang")
            return slangCompiler;
        // Intentional fallthrough...
#endif
#if VEX_DXC
    case ShaderCompilerBackend::DXC:
        return dxcCompiler;
#endif
#if VEX_SLANG
    case ShaderCompilerBackend::Slang:
        return slangCompiler;
#endif
    default:
        VEX_LOG(Fatal,
                "Invalid shader compiler backend when attempting to obtain compiler, extension: {} with key: {}.",
                extension,
                key.filepath);
        std::unreachable();
    }
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key, bool allowHotReload)
{
    if (const auto shader = shaderCache.find(key); shader != shaderCache.end())
    {
        return shader->second;
    }

    // Avoids the default constructor being called (it is not defined for class Shader)
    shaderCache.emplace(std::piecewise_construct, std::make_tuple(key), std::make_tuple(Shader(key)));
    const NonNullPtr shader = shaderCache.find(key)->second;
    if (allowHotReload && compilerSettings.enableShaderHotReload && VEX_HAS_AT_LEAST_ONE_COMPILER)
    {
        // If the shader does not yet exist, attempt to compile it via the hotreload path.
        std::ignore = HotReloadShaders(Span<const ShaderKey>{ key });
    }
    return shader;
}

std::optional<std::string> ShaderCompiler::HandleCompiledShader(
    Shader& shader, std::expected<ShaderCompilationResult, std::string>&& compilationResult) const
{
    shader.isErrored = false;

    if (!compilationResult.has_value())
    {
        shader.isErrored = true;
        return compilationResult.error();
    }

    // Store shader compilation result inside the Shader.
    shader.res = std::move(compilationResult.value());

    // Outputs raw bytecode if we ever need to figure out a difficult shader issue (for spirv can use
    // spirv-dis to read raw .spv file, similar tools like RenderDoc work for dxil).
    if (compilerSettings.dumpShaderOutputBytecode)
    {
        const std::vector<byte>& shaderBytecode = compilationResult->compiledCode;

        std::stringstream metadataStream;
        metadataStream << "Hash: " << HashToString(shader.GetHash()) << "\n";
        metadataStream << "Type: " << magic_enum::enum_name(shader.GetKey().type) << "\n";
        metadataStream << "Defines: \n";
        for (const auto& define : shader.GetKey().defines)
        {
            metadataStream << "\t" << define.name << " = " << define.value << "\n";
        }

        std::filesystem::path outputPath = ShaderUtil::GetShaderDumpPath(shader);
        std::filesystem::path bytecodePath = outputPath;
        std::filesystem::path metaPath = outputPath;
        metaPath.replace_extension(".meta");

        if (compilerSettings.target == CompilationTarget::SPIRV)
        {
            bytecodePath.replace_extension(".spv"); // or ".spirv"
        }
        else if (compilerSettings.target == CompilationTarget::DXIL)
        {
            bytecodePath.replace_extension(".dxil");
        }

        if (!std::filesystem::exists(outputPath.parent_path()))
        {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        bool fileError = false;

        if (std::ofstream ofstream(bytecodePath, std::ios::binary); ofstream)
        {
            ofstream.write(reinterpret_cast<const char*>(shaderBytecode.data()), shaderBytecode.size());
        }
        else
        {
            fileError = true;
        }

        if (std::ofstream ofstream(metaPath, std::ios::binary); ofstream)
        {
            ofstream << metadataStream.str();
        }
        else
        {
            fileError = true;
        }

        if (fileError)
        {
            VEX_LOG(Error, "Failed to write shader bytecode to: {}", bytecodePath.string());
        }
        else
        {
            VEX_LOG(Info, "Shader bytecode written to: {}", bytecodePath.string());
        }
    }

    return std::nullopt;
}

bool ShaderCompiler::IsShaderStale(const Shader& shader) const
{
    // TODO(https://trello.com/c/UquJz7ow): figure out a better way to determine which shaders are due for recompilation
    // For now all shaders are considered stale when asked
    return true;
}

bool ShaderCompiler::HandleCompilationErrors(Span<const std::pair<ShaderKey, std::string>> shaderKeysAndErrors)
{
    if (shaderKeysAndErrors.empty())
    {
        return false;
    }

    std::vector<ShaderKey> shaderKeys;
    shaderKeys.reserve(shaderKeysAndErrors.size());
    std::string errorMessage{ std::format("Error compiling shader{}:\n\t",
                                          shaderKeysAndErrors.size() > 1 ? "(s)" : "") };

    for (auto& [key, error] : shaderKeysAndErrors)
    {
        errorMessage.append(std::format("- {}:\n\t- Reason: {}", key, error));
    }

    // If we're not in a hot-reload context, a non-compiling shader is fatal.
    bool displayFatalError = !compilerSettings.enableShaderHotReload;

    if (errorsCallback)
    {
        switch (errorsCallback(shaderKeysAndErrors))
        {
        case ShaderHotReloadErrorResponse::RetryAndRetainPreviousShader:
            return true;
        case ShaderHotReloadErrorResponse::RetainPreviousShader:
            break;
        case ShaderHotReloadErrorResponse::Abort:
            displayFatalError = true;
            break;
        default:
            break;
        }
    }

    VEX_LOG(displayFatalError ? Fatal : Error, "{}", errorMessage);

    return false;
}

template <std::ranges::input_range TRange>
    requires std::same_as<std::ranges::range_value_t<TRange>, ShaderKey>
u32 ShaderCompiler::HotReloadShaders(TRange&& shaderKeys)
{
    if (!compilerSettings.enableShaderHotReload)
    {
        VEX_LOG(Warning, "Cannot recompile shaders when shader hot reload is not enabled.");
        return 0;
    }

    std::vector<ShaderKey> shadersLeftToCompile = { shaderKeys.begin(), shaderKeys.end() };
    std::vector<std::pair<ShaderKey, std::string>> errors;
    u32 numRecompiledShaders;

    do
    {
        numRecompiledShaders = 0;
        errors.clear();
        std::erase_if(shadersLeftToCompile,
                      [&](const ShaderKey& key) -> bool
                      {
                          // Attempt to recompile the shader.
                          if (std::optional<std::string> res = CompileShaderFromFilepath(key); res.has_value())
                          {
                              errors.emplace_back(key, res.value());
                              return false;
                          }

                          ++numRecompiledShaders;
                          return true;
                      });
    }
    while (HandleCompilationErrors(errors));

    return numRecompiledShaders;
}

} // namespace vex::sc
