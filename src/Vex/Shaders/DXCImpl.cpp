#include "DXCImpl.h"

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

static std::vector<DxcDefine> ConvertDefinesToDxcDefine(const std::vector<ShaderDefine>& defines)
{
    std::vector<DxcDefine> dxcDefines;
    dxcDefines.reserve(defines.size());
    for (auto& d : defines)
    {
        dxcDefines.emplace_back(DxcDefine{ .Name = d.name.c_str(), .Value = d.value.c_str() });
    }
    return dxcDefines;
}

static void FillInAdditionalIncludeDirectories(std::vector<LPCWSTR>& args,
                                               std::span<const std::filesystem::path> additionalIncludeDirectories)
{
    for (auto& additionalIncludeFolder : additionalIncludeDirectories)
    {
        args.insert(args.end(), { L"-I", StringToWString(additionalIncludeFolder.string()).c_str() });
    }
}

} // namespace DXCImpl_Internal

DXCCompilerImpl::DXCCompilerImpl()
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

ComPtr<IDxcResult> DXCCompilerImpl::GetPreprocessedShader(
    const Shader& shader,
    const ComPtr<IDxcBlobEncoding>& shaderBlobUTF8,
    std::span<const std::filesystem::path> additionalIncludeDirectories) const
{
    std::vector<LPCWSTR> args = { L"-P" }; // -P gives us the preprocessor output of the shader.
    DXCImpl_Internal::FillInAdditionalIncludeDirectories(args, additionalIncludeDirectories);

    DxcBuffer buffer = { .Ptr = shaderBlobUTF8->GetBufferPointer(),
                         .Size = shaderBlobUTF8->GetBufferSize(),
                         .Encoding = CP_UTF8 };

    ComPtr<IDxcResult> result;
    if (HRESULT hr =
            compiler->Compile(&buffer, args.data(), static_cast<u32>(args.size()), nullptr, IID_PPV_ARGS(&result));
        FAILED(hr))
    {
        return nullptr;
    }

    return result;
}

std::expected<std::vector<u8>, std::string> DXCCompilerImpl::CompileShader(
    std::string& shaderFileStr,
    ShaderEnvironment& shaderEnv,
    std::span<const std::filesystem::path> additionalIncludeDirectories,
    const ShaderCompilerSettings& compilerSettings,
    const Shader& shader) const
{
    using namespace DXCImpl_Internal;

    ComPtr<IDxcBlobEncoding> shaderBlob;
    if (HRESULT hr = utils->CreateBlobFromPinned(shaderFileStr.c_str(), shaderFileStr.size(), CP_UTF8, &shaderBlob);
        FAILED(hr))
    {
        return std::unexpected("Failed to create blob from shader.");
    }

    DxcBuffer shaderSource = {};
    shaderSource.Ptr = shaderBlob->GetBufferPointer();
    shaderSource.Size = shaderBlob->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    std::vector<LPCWSTR> args;
    args.reserve(shaderEnv.args.size());
    for (const std::wstring& arg : shaderEnv.args)
    {
        args.emplace_back(arg.c_str());
    }

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

    FillInAdditionalIncludeDirectories(args, additionalIncludeDirectories);

    std::vector<DxcDefine> dxcDefines = ConvertDefinesToDxcDefine(shaderEnv.defines);

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
    std::vector<u8> finalShaderBlob;
    finalShaderBlob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(finalShaderBlob.data(), shaderBytecode->GetBufferPointer(), finalShaderBlob.size() * sizeof(u8));

    // TODO(https://trello.com/c/9D3KOgHr): Reflection blob, to be implemented later on.
    {
        // ComPtr<IDxcBlob> reflectionBlob;
        // HRESULT reflectionResult =
        //     shaderCompilationResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob),
        //     nullptr);
        // if (FAILED(reflectionResult))
        // {
        //     return std::unexpected("Unable to get the shader reflection blob after compilation.");
        // }
        //
        // DxcBuffer reflectionBuffer;
        // reflectionBuffer.Encoding = DXC_CP_ACP;
        // reflectionBuffer.Ptr = reflectionBlob->GetBufferPointer();
        // reflectionBuffer.Size = reflectionBlob->GetBufferSize();

        // ComPtr<ID3D12ShaderReflection> reflectionData;
        // chk << compilerUtil.utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflectionData));

        // D3D12_SHADER_DESC reflectionDesc;
        // reflectionData->GetDesc(&reflectionDesc);
        // for (uint32 i = 0; i < reflectionDesc.BoundResources; ++i)
        //{
        //     D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        //     reflectionData->GetResourceBindingDesc(i, &bindDesc);

        //    switch (bindDesc.Type)
        //    {
        //    case D3D_SIT_CBUFFER:
        //    case D3D_SIT_TEXTURE:
        //    case D3D_SIT_STRUCTURED:
        //    case D3D_SIT_BYTEADDRESS:
        //    case D3D_SIT_UAV_RWTYPED:
        //    case D3D_SIT_UAV_RWSTRUCTURED:
        //    case D3D_SIT_UAV_RWBYTEADDRESS:
        //        cbvSrvUavReflectionMap[std::string(bindDesc.Name)] = bindDesc.BindPoint;
        //        break;
        //    case D3D_SIT_SAMPLER:
        //        samplerReflectionMap[std::string(bindDesc.Name)] = bindDesc.BindPoint;
        //        break;
        //    default:
        //        break;
        //    }
        //}
    }

    return finalShaderBlob;
}

std::optional<std::size_t> DXCCompilerImpl::GetShaderHash(
    const Shader& shader, std::span<const std::filesystem::path> additionalIncludeDirectories) const
{
    ComPtr<IDxcBlobEncoding> shaderBlobUTF8;
    u32 codePage = CP_UTF8;
    if (HRESULT hr = utils->LoadFile(shader.key.path.wstring().c_str(), &codePage, &shaderBlobUTF8); FAILED(hr))
    {
        VEX_LOG(Error, "Unable to get shader hash, failed to load shader from filepath: {}", shader.key.path.string());
        return std::nullopt;
    }

    if (ComPtr<IDxcResult> preprocessingResult =
            GetPreprocessedShader(shader, shaderBlobUTF8, additionalIncludeDirectories))
    {
        ComPtr<IDxcBlob> preprocessedShader;
        if (HRESULT hr = preprocessingResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&preprocessedShader), nullptr);
            SUCCEEDED(hr))
        {
            std::string_view finalSourceView{ static_cast<const char*>(preprocessedShader->GetBufferPointer()),
                                              preprocessedShader->GetBufferSize() };
            return std::hash<std::string_view>{}(finalSourceView);
        }
    }

    return std::nullopt;
}

} // namespace vex