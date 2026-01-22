#include "DXCImpl.h"

#include <algorithm>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Shaders/DXCImplReflection.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Utility/SHA1.h>

namespace vex
{

namespace DXCImpl_Internal
{

static constexpr std::array HLSL202xFlags{
    L"-HV 202x", L"-Wconversion", L"-Wdouble-promotion", L"-Whlsl-legacy-literal"
};

static std::wstring GetTargetFromShaderType(ShaderType type)
{
    std::wstring highestSupportedShaderModel =
        StringToWString(std::string(magic_enum::enum_name(GPhysicalDevice->GetShaderModel())));

    using enum ShaderType;

    // First char changes depending on shader type.
    switch (type)
    {
    case VertexShader:
        highestSupportedShaderModel[0] = L'v';
        break;
    case PixelShader:
        highestSupportedShaderModel[0] = L'p';
        break;
    case ComputeShader:
        highestSupportedShaderModel[0] = L'c';
        break;
    // Raytracing shaders use "lib_*" target profile
    case RayGenerationShader:
    case RayMissShader:
    case RayClosestHitShader:
    case RayAnyHitShader:
    case RayIntersectionShader:
    case RayCallableShader:
        return L"lib" + highestSupportedShaderModel.substr(2);
    default:
        VEX_LOG(Fatal, "Unsupported shader type for the Vex ShaderCompiler.");
        break;
    }

    // Second character is always 's' for non-RT shaders.
    highestSupportedShaderModel[1] = L's';

    return highestSupportedShaderModel;
}

std::expected<ComPtr<IDxcBlobEncoding>, std::string> LoadShaderSource(const ComPtr<IDxcUtils>& utils,
                                                                      const ShaderKey& key)
{
    ComPtr<IDxcBlobEncoding> shaderBlob;

    if (!key.path.empty())
    {
        std::wstring shaderPath = StringToWString(key.path.string());
        if (HRESULT hr = utils->LoadFile(shaderPath.c_str(), nullptr, &shaderBlob); FAILED(hr))
        {
            return std::unexpected(
                std::format("Failed to load shader from filesystem at path: {}.", key.path.string()));
        }
    }
    else
    {
        if (HRESULT hr = utils->CreateBlob(key.sourceCode.c_str(), key.sourceCode.size(), DXC_CP_ACP, &shaderBlob);
            FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create shader blob from source string: {}.", key.sourceCode));
        }
    }

    return shaderBlob;
}

std::expected<SHA1HashDigest, std::string> HashCompiledShader(const ComPtr<IDxcResult>& compiledShader)
{
    ComPtr<IDxcBlob> shaderHLSLCode;
    HRESULT objectResult = compiledShader->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&shaderHLSLCode), nullptr);
    if (FAILED(objectResult))
    {
        return std::unexpected("Failed to obtain the shader hlsl blob after preprocess.");
    }

    // Use the preprocessed code
    const char* preprocessedCode = (const char*)shaderHLSLCode->GetBufferPointer();
    size_t codeSize = shaderHLSLCode->GetBufferSize();

    std::string stringCode{ preprocessedCode, codeSize };

    SHA1 sha1;
    sha1.update(stringCode);
    return sha1.final();
}

std::vector<std::wstring> BuildDefaultArgumentList(const ShaderCompilerSettings& compilerSettings,
                                                   const std::vector<std::filesystem::path>& includeDirectories)
{
    std::vector<std::wstring> args;
    if (compilerSettings.enableShaderDebugging)
    {
        args.insert(args.end(),
                    {
                        DXC_ARG_DEBUG,
                        DXC_ARG_WARNINGS_ARE_ERRORS,
                        DXC_ARG_DEBUG_NAME_FOR_SOURCE,
                        L"-Qembed_debug",
                    });
    }

#if VEX_VULKAN
    std::string_view vulkanVersion = GPhysicalDevice->GetMaxSupportedVulkanVersion();
    std::wstring vulkanVersionFlag = std::format(L"-fspv-target-env={}", StringToWString(std::string(vulkanVersion)));
    args.emplace_back(L"-spirv");
    args.emplace_back(L"-fvk-bind-resource-heap");
    args.emplace_back(L"0");
    args.emplace_back(L"1");
    args.emplace_back(vulkanVersionFlag.c_str());

    // Flags to keep Vk similar to DX12 hlsl conventions.
    args.emplace_back(L"-fvk-use-dx-layout");
    args.emplace_back(L"-fvk-support-nonzero-base-instance");
    args.emplace_back(L"-fvk-support-nonzero-base-vertex");
    args.emplace_back(L"-fspv-reflect");
#endif

#if VEX_DX12
    args.emplace_back(L"-Qstrip_reflect");
#endif

    if (compilerSettings.enableHLSL202xFeatures)
    {
        args.insert(args.end(), HLSL202xFlags.begin(), HLSL202xFlags.end());
    }

    std::vector<std::wstring> wStringPaths;
    wStringPaths.reserve(includeDirectories.size() + 1);
    for (auto& additionalIncludeFolder : includeDirectories)
    {
        wStringPaths.emplace_back(StringToWString(additionalIncludeFolder.string()));
        args.insert(args.end(), { L"-I", wStringPaths.back().c_str() });
    }

    // Also adds the current working directory for the Vex.hlsli file.
    wStringPaths.emplace_back(StringToWString(std::filesystem::current_path().string()));
    args.insert(args.end(), { L"-I", wStringPaths.back().c_str() });

    return args;
}

std::vector<std::pair<std::wstring, std::wstring>> BuildDefineList(const ShaderKey& key,
                                                                   const ShaderEnvironment& shaderEnv)
{
    // Fill in the defines with both the shaderEnv and the shader key's defines.
    std::vector<std::pair<std::wstring, std::wstring>> defineWStrings;
    defineWStrings.reserve(shaderEnv.defines.size() + key.defines.size());
    std::ranges::transform(shaderEnv.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });
    std::ranges::transform(key.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });

    return defineWStrings;
}

} // namespace DXCImpl_Internal

DXCCompilerImpl::DXCCompilerImpl(std::vector<std::filesystem::path> includeDirectories)
    : CompilerBase(std::move(includeDirectories))
{
    // Create compiler and utils.
    HRESULT hr1 = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    HRESULT hr2 = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    if (FAILED(hr1) || FAILED(hr2))
    {
        VEX_LOG(Fatal, "Failed to create DxcCompiler and/or DxcUtils...");
    }

    // Create default include handler.
    HRESULT hr3 = utils->CreateDefaultIncludeHandler(&defaultIncludeHandler);

    if (FAILED(hr3))
    {
        VEX_LOG(Fatal, "Failed to create default include handler...");
    }
}

DXCCompilerImpl::~DXCCompilerImpl() = default;

std::expected<SHA1HashDigest, std::string> DXCCompilerImpl::GetShaderCodeHash(
    const Shader& shader, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings)
{
    return DXCImpl_Internal::LoadShaderSource(utils, shader.key)
        .and_then(
            [&](const ComPtr<IDxcBlobEncoding>& shaderBlob)
            {
                DxcBuffer shaderSource = {};
                shaderSource.Ptr = shaderBlob->GetBufferPointer();
                shaderSource.Size = shaderBlob->GetBufferSize();
                // Assume BOM says UTF8 or UTF16 or this is ANSI text.
                shaderSource.Encoding = DXC_CP_ACP;

                std::vector<std::wstring> args =
                    DXCImpl_Internal::BuildDefaultArgumentList(compilerSettings, includeDirectories);
                // -P resolves the pre-processor, allowing us to hash the result.
                args.push_back(L"-P");
                std::vector<std::pair<std::wstring, std::wstring>> dxcDefines =
                    DXCImpl_Internal::BuildDefineList(shader.key, shaderEnv);

                return CompileShader(shader.key, args, dxcDefines, shaderSource)
                    .and_then(DXCImpl_Internal::HashCompiledShader);
            });
}

std::expected<ShaderCompilationResult, std::string> DXCCompilerImpl::CompileShader(
    const Shader& shader, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const
{
    return DXCImpl_Internal::LoadShaderSource(utils, shader.key)
        .and_then(
            [&](const ComPtr<IDxcBlobEncoding>& shaderBlob)
            {
                DxcBuffer shaderSource = {};
                shaderSource.Ptr = shaderBlob->GetBufferPointer();
                shaderSource.Size = shaderBlob->GetBufferSize();
                // Assume BOM says UTF8 or UTF16 or this is ANSI text.
                shaderSource.Encoding = DXC_CP_ACP;

                std::vector<std::wstring> args =
                    DXCImpl_Internal::BuildDefaultArgumentList(compilerSettings, includeDirectories);
                std::vector<std::pair<std::wstring, std::wstring>> dxcDefines =
                    DXCImpl_Internal::BuildDefineList(shader.key, shaderEnv);

                return CompileShader(shader.key, args, dxcDefines, shaderSource)
                    .and_then(
                        [&](const ComPtr<IDxcResult>& shaderCompilationResults)
                            -> std::expected<ShaderCompilationResult, std::string>
                        {
                            ComPtr<IDxcBlob> shaderBytecode;
                            HRESULT objectResult = shaderCompilationResults->GetOutput(DXC_OUT_OBJECT,
                                                                                       IID_PPV_ARGS(&shaderBytecode),
                                                                                       nullptr);
                            if (FAILED(objectResult))
                            {
                                return std::unexpected("Failed to obtain the shader blob after compilation.");
                            }

                            // Store shader bytecode blob inside the Shader.
                            std::vector<byte> finalShaderBlob;
                            finalShaderBlob.resize(shaderBytecode->GetBufferSize());
                            std::memcpy(finalShaderBlob.data(),
                                        shaderBytecode->GetBufferPointer(),
                                        finalShaderBlob.size() * sizeof(u8));

                            std::optional<ShaderReflection> reflection{};
                            if (ShaderUtil::CanReflectShaderType(shader.key.type))
                            {
#if VEX_VULKAN
                                reflection = GetSpirvReflection(finalShaderBlob);
#endif
#if VEX_DX12
                                reflection = GetDxcReflection(shaderCompilationResults);
#endif
                            }

                            return ShaderCompilationResult{ {}, finalShaderBlob, reflection };
                        });
            });
}

std::expected<ComPtr<IDxcResult>, std::string> DXCCompilerImpl::CompileShader(
    const ShaderKey& key,
    const std::vector<std::wstring>& args,
    const std::vector<std::pair<std::wstring, std::wstring>>& defines,
    const DxcBuffer& shaderSource) const
{
    std::wstring entryPoint;
    // RT Shaders are compiled differently to other shader types, the entry point should be null.
    if (!IsRayTracingShader(key.type))
    {
        entryPoint = StringToWString(key.entryPoint);
    }

    std::vector<LPCWSTR> wstrArgs;
    std::transform(args.begin(),
                   args.end(),
                   std::back_inserter(wstrArgs),
                   [](const std::wstring& str) { return str.data(); });

    // Convert to DxcDefine type.
    std::vector<DxcDefine> dxcDefines;
    dxcDefines.reserve(defines.size());
    std::ranges::transform(defines,
                           std::back_inserter(dxcDefines),
                           [](const std::pair<std::wstring, std::wstring>& d)
                           { return DxcDefine{ .Name = d.first.c_str(), .Value = d.second.c_str() }; });

    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (HRESULT hr = utils->BuildArguments(key.path.wstring().c_str(),
                                           entryPoint.empty() ? nullptr : entryPoint.c_str(),
                                           DXCImpl_Internal::GetTargetFromShaderType(key.type).c_str(),
                                           wstrArgs.data(),
                                           static_cast<u32>(args.size()),
                                           dxcDefines.data(),
                                           static_cast<u32>(dxcDefines.size()),
                                           &compilerArgs);
        FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to build shader compilation arguments from file: {}", key.path.string()));
    }

    ComPtr<IDxcResult> shaderCompilationResults;
    (void)compiler->Compile(&shaderSource,
                            compilerArgs->GetArguments(),
                            compilerArgs->GetCount(),
                            defaultIncludeHandler.operator->(),
                            IID_PPV_ARGS(&shaderCompilationResults));

    ComPtr<IDxcBlobUtf8> errors = nullptr;
    if (HRESULT hrError = shaderCompilationResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        FAILED(hrError))
    {
        return std::unexpected("Failed to compile shader due to unknown reasons, the DXC compilation error "
                               "was unable to be obtained.");
    }
    if (errors != nullptr && errors->GetStringLength() != 0)
    {
        return std::unexpected(std::format("{}", errors->GetStringPointer()));
    }
    return shaderCompilationResults;
}

} // namespace vex