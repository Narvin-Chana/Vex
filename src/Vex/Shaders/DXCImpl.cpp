#include "DXCImpl.h"

#include <algorithm>
#include <string_view>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderKey.h>

namespace vex
{

namespace DXCImpl_Internal
{

static constexpr std::array HLSL202xFlags{
    L"-HV 202x", L"-Wconversion", L"-Wdouble-promotion", L"-Whlsl-legacy-literal"
};

static std::wstring GetTargetFromShaderType(ShaderType type)
{
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();

    std::wstring highestSupportedShaderModel =
        StringToWString(std::string(magic_enum::enum_name(featureChecker->GetShaderModel())));

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
    case ShaderType::RayGenerationShader:
    case ShaderType::RayMissShader:
    case ShaderType::RayClosestHitShader:
    case ShaderType::RayAnyHitShader:
    case ShaderType::RayIntersectionShader:
    case ShaderType::RayCallableShader:
        return L"lib" + highestSupportedShaderModel.substr(2);
    default:
        VEX_LOG(Fatal, "Unsupported shader type for the Vex ShaderCompiler.");
        break;
    }

    // Second character is always 's' for non-RT shaders.
    highestSupportedShaderModel[1] = L's';

    return highestSupportedShaderModel;
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

std::expected<std::vector<byte>, std::string> DXCCompilerImpl::CompileShader(
    const Shader& shader, ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const
{
    using namespace DXCImpl_Internal;

    ComPtr<IDxcBlobEncoding> shaderBlob;
    std::wstring shaderPath = StringToWString(shader.key.path.string());
    if (HRESULT hr = utils->LoadFile(shaderPath.c_str(), nullptr, &shaderBlob); FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to load shader from filesystem at path: {}.", shader.key.path.string()));
    }

    DxcBuffer shaderSource = {};
    shaderSource.Ptr = shaderBlob->GetBufferPointer();
    shaderSource.Size = shaderBlob->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    std::vector<LPCWSTR> args;
    args.reserve(shaderEnv.args.size());
    std::ranges::transform(shaderEnv.args,
                           std::back_inserter(args),
                           [](const std::wstring& arg) { return arg.c_str(); });

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

    if (compilerSettings.enableHLSL202xFeatures)
    {
        args.insert(args.end(), HLSL202xFlags.begin(), HLSL202xFlags.end());
    }

    std::vector<std::wstring> wStringPaths;
    FillInIncludeDirectories(args, wStringPaths);

    // Fill in the defines with both the shaderEnv and the shader key's defines.
    std::vector<std::pair<std::wstring, std::wstring>> defineWStrings;
    defineWStrings.reserve(shaderEnv.defines.size() + shader.key.defines.size());
    std::ranges::transform(shaderEnv.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });
    std::ranges::transform(shader.key.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });

    // Convert to DxcDefine type.
    std::vector<DxcDefine> dxcDefines;
    dxcDefines.reserve(shaderEnv.defines.size() + shader.key.defines.size());
    std::ranges::transform(defineWStrings,
                           std::back_inserter(dxcDefines),
                           [](const std::pair<std::wstring, std::wstring>& d)
                           { return DxcDefine{ .Name = d.first.c_str(), .Value = d.second.c_str() }; });

    std::wstring entryPoint;
    // RT Shaders are compiled differently to other shader types, the entry point should be null.
    if (!IsRayTracingShader(shader.key.type))
    {
        entryPoint = StringToWString(shader.key.entryPoint);
    }

    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (HRESULT hr = utils->BuildArguments(shader.key.path.wstring().c_str(),
                                           entryPoint.empty() ? nullptr : entryPoint.c_str(),
                                           GetTargetFromShaderType(shader.key.type).c_str(),
                                           args.data(),
                                           static_cast<u32>(args.size()),
                                           dxcDefines.data(),
                                           static_cast<u32>(dxcDefines.size()),
                                           &compilerArgs);
        FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to build shader compilation arguments from file: {}", shader.key.path.string()));
    }

    ComPtr<IDxcResult> shaderCompilationResults;
    HRESULT compilationHR = compiler->Compile(&shaderSource,
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

    ComPtr<IDxcBlob> shaderBytecode;
    HRESULT objectResult = shaderCompilationResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBytecode), nullptr);
    if (FAILED(objectResult))
    {
        return std::unexpected("Failed to obtain the shader blob after compilation.");
    }

    // Store shader bytecode blob inside the Shader.
    std::vector<byte> finalShaderBlob;
    finalShaderBlob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(finalShaderBlob.data(), shaderBytecode->GetBufferPointer(), finalShaderBlob.size() * sizeof(u8));

    return finalShaderBlob;
}

void DXCCompilerImpl::FillInIncludeDirectories(std::vector<LPCWSTR>& args, std::vector<std::wstring>& wStrings) const
{
    wStrings.reserve(includeDirectories.size() + 1);
    for (auto& additionalIncludeFolder : includeDirectories)
    {
        wStrings.emplace_back(StringToWString(additionalIncludeFolder.string()));
        args.insert(args.end(), { L"-I", wStrings.back().c_str() });
    }

    // Also adds the current working directory for the Vex.hlsli file.
    wStrings.emplace_back(StringToWString(std::filesystem::current_path().string()));
    args.insert(args.end(), { L"-I", wStrings.back().c_str() });
}

} // namespace vex