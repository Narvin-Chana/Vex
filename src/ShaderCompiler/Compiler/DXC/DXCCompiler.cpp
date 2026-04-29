#include "DXCCompiler.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>

#include <Vex/Logger.h>
#include <Vex/ShaderView.h>
#include <Vex/Utility/SHA1.h>

#include <ShaderCompiler/Compiler/DXC/DXCReflection.h>
#include <ShaderCompiler/Shader.h>
#include <ShaderCompiler/ShaderCompiler.h>
#include <ShaderCompiler/ShaderCompilerSettings.h>
#include <ShaderCompiler/ShaderEnvironment.h>
#include <ShaderCompiler/ShaderKey.h>

namespace vex::sc
{

namespace DXCCompiler_Internal
{

static std::wstring GetTargetFromShaderType(const ShaderType type, const ShaderCompilerSettings& compilerSettings)
{
    std::wstring highestSupportedShaderModel =
        StringToWString(std::string(magic_enum::enum_name(compilerSettings.shaderModel)));

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
                                                                      const std::filesystem::path& filepath)
{
    ComPtr<IDxcBlobEncoding> shaderBlob;
    const std::wstring shaderPath = StringToWString(filepath.string());
    if (const HRESULT hr = utils->LoadFile(shaderPath.c_str(), nullptr, &shaderBlob); FAILED(hr))
    {
        return std::unexpected(std::format("Failed to load shader from filesystem at path: {}.", filepath.string()));
    }
    return shaderBlob;
}

std::expected<ComPtr<IDxcBlobEncoding>, std::string> LoadShaderSource(const ComPtr<IDxcUtils>& utils,
                                                                      std::string_view sourceCode)
{
    ComPtr<IDxcBlobEncoding> shaderBlob;
    if (const HRESULT hr = utils->CreateBlob(sourceCode.data(), sourceCode.size(), DXC_CP_ACP, &shaderBlob); FAILED(hr))
    {
        return std::unexpected(std::format("Failed to create shader blob from source string: {}.", sourceCode));
    }
    return shaderBlob;
}

std::expected<SHA1HashDigest, std::string> HashCompiledShader(const ComPtr<IDxcResult>& compiledShader)
{
    ComPtr<IDxcBlob> shaderHLSLCode;
    const HRESULT objectResult = compiledShader->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&shaderHLSLCode), nullptr);
    if (FAILED(objectResult))
    {
        return std::unexpected("Failed to obtain the shader hlsl blob after preprocess.");
    }

    // Use the preprocessed code
    const char* preprocessedCode = static_cast<const char*>(shaderHLSLCode->GetBufferPointer());
    const size_t codeSize = shaderHLSLCode->GetBufferSize();

    const std::string stringCode{ preprocessedCode, codeSize };

    SHA1 sha1;
    sha1.update(stringCode);
    return sha1.final();
}

std::vector<std::wstring> BuildDefaultArgumentList(const ShaderCompilerSettings& compilerSettings,
                                                   const std::vector<std::filesystem::path>& includeDirectories)
{
    std::vector<std::wstring> args{
        L"-HV 202x",
        L"-Wconversion",
        L"-Wdouble-promotion",
        DXC_ARG_WARNINGS_ARE_ERRORS,
    };
    if (compilerSettings.enableShaderDebugSymbols)
    {
        args.insert(args.end(),
                    {
                        DXC_ARG_DEBUG,
                        DXC_ARG_DEBUG_NAME_FOR_SOURCE,
                        L"-Qembed_debug",
                    });
    }
    if (!compilerSettings.enableShaderOptimizations)
    {
        args.emplace_back(DXC_ARG_SKIP_OPTIMIZATIONS);
    }

    if (compilerSettings.target == CompilationTarget::SPIRV)
    {
        std::string_view vulkanVersion;
        switch(compilerSettings.spirvVersion)
        {
        case SpirvVersion::spirv_1_0:
        case SpirvVersion::spirv_1_1:
        case SpirvVersion::spirv_1_2:
            vulkanVersion = "vulkan1.0";
            break;
        case SpirvVersion::spirv_1_3:
            vulkanVersion = "vulkan1.1";
            break;
        case SpirvVersion::spirv_1_4:
            vulkanVersion = "vulkan1.1spirv1.4";
            break;
        case SpirvVersion::spirv_1_5:
        case SpirvVersion::spirv_1_6:
            vulkanVersion = "vulkan1.3";
            break;
        }
        std::wstring spirvVersionFlag = std::format(L"-fspv-target-env={}", StringToWString(std::string(vulkanVersion)));
        args.emplace_back(L"-spirv");
        args.emplace_back(L"-fvk-bind-resource-heap");
        args.emplace_back(L"0");
        args.emplace_back(L"0");
        args.emplace_back(L"-fvk-bind-sampler-heap");
        args.emplace_back(L"0");
        args.emplace_back(L"0");

        args.emplace_back(spirvVersionFlag.c_str());

        // Flags to keep Vk similar to DX12 hlsl conventions.
        args.emplace_back(L"-fvk-use-dx-layout");
        args.emplace_back(L"-fvk-support-nonzero-base-instance");
        args.emplace_back(L"-fvk-support-nonzero-base-vertex");
        args.emplace_back(L"-fspv-reflect");
    }
    else if (compilerSettings.target == CompilationTarget::DXIL)
    {
        args.emplace_back(L"-Qstrip_reflect");
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
                                                                   const ShaderEnvironment& environment)
{
    // Fill in the defines with both the shaderEnv and the shader key's defines.
    std::vector<std::pair<std::wstring, std::wstring>> defineWStrings;
    defineWStrings.reserve(environment.defines.size() + key.defines.size());
    std::ranges::transform(environment.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });
    std::ranges::transform(key.defines,
                           std::back_inserter(defineWStrings),
                           [](const ShaderDefine& d) -> std::pair<std::wstring, std::wstring>
                           { return { StringToWString(d.name), StringToWString(d.value) }; });

    return defineWStrings;
}

} // namespace DXCCompiler_Internal

DXCCompiler::DXCCompiler(std::vector<std::filesystem::path> includeDirectories)
    : CompilerBase(std::move(includeDirectories))
{
    // Create compiler and utils.
    const HRESULT hr1 = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    const HRESULT hr2 = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if (FAILED(hr1) || FAILED(hr2))
    {
        VEX_LOG(Fatal, "Failed to create DxcCompiler and/or DxcUtils...");
    }

    // Create default include handler.
    const HRESULT hr3 = utils->CreateDefaultIncludeHandler(&defaultIncludeHandler);
    if (FAILED(hr3))
    {
        VEX_LOG(Fatal, "Failed to create default include handler...");
    }
}

std::expected<ShaderCompilationResult, std::string> DXCCompiler::CompileShader(
    const Shader& shader,
    const std::filesystem::path& filepath,
    const ShaderEnvironment& environment,
    const ShaderCompilerSettings& compilerSettings)
{
    return DXCCompiler_Internal::LoadShaderSource(utils, filepath)
        .and_then([&](const ComPtr<IDxcBlobEncoding>& blob)
                  { return CompileShaderFromBlob(shader, blob, environment, compilerSettings); });
}

std::expected<ShaderCompilationResult, std::string> DXCCompiler::CompileShader(
    const Shader& shader,
    const std::string_view sourceCode,
    const ShaderEnvironment& environment,
    const ShaderCompilerSettings& compilerSettings)
{
    return DXCCompiler_Internal::LoadShaderSource(utils, sourceCode)
        .and_then([&](const ComPtr<IDxcBlobEncoding>& blob)
                  { return CompileShaderFromBlob(shader, blob, environment, compilerSettings); });
}

std::expected<SHA1HashDigest, std::string> DXCCompiler::GetShaderCodeHash(
    const Shader& shader,
    const DxcBuffer& shaderSource,
    const ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings) const
{
    std::vector<std::wstring> args =
        DXCCompiler_Internal::BuildDefaultArgumentList(compilerSettings, includeDirectories);
    // -P resolves the pre-processor, allowing us to hash the result.
    args.push_back(L"-P");
    const std::vector<std::pair<std::wstring, std::wstring>> dxcDefines =
        DXCCompiler_Internal::BuildDefineList(shader.GetKey(), shaderEnv);

    return CompileShaderFromBuffer(shader.GetKey(), shaderSource, args, dxcDefines, compilerSettings)
        .and_then(DXCCompiler_Internal::HashCompiledShader);
}

std::expected<ShaderCompilationResult, std::string> DXCCompiler::CompileShaderFromBlob(
    const Shader& shader,
    const ComPtr<IDxcBlobEncoding>& shaderSourceBlob,
    const ShaderEnvironment& environment,
    const ShaderCompilerSettings& compilerSettings)
{
    DxcBuffer shaderSourceBuffer = {};
    shaderSourceBuffer.Ptr = shaderSourceBlob->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSourceBlob->GetBufferSize();
    // Assume BOM says UTF8 or UTF16 or this is ANSI text.
    shaderSourceBuffer.Encoding = DXC_CP_ACP;

    std::expected<SHA1HashDigest, std::string> shaderHash =
        GetShaderCodeHash(shader, shaderSourceBuffer, environment, compilerSettings);
    if (!shaderHash.has_value())
    {
        return std::unexpected(shaderHash.error());
    }

    if (shader.IsValid() && shaderHash.value() == shader.GetHash())
    {
        // Nothing to recompile, return the already existing shader.
        return ShaderCompilationResult{ shader.GetHash(),
                                        { shader.GetBlob().begin(), shader.GetBlob().end() },
                                        *shader.GetReflection() };
    }

    const std::vector<std::wstring> args =
        DXCCompiler_Internal::BuildDefaultArgumentList(compilerSettings, includeDirectories);
    const std::vector<std::pair<std::wstring, std::wstring>> dxcDefines =
        DXCCompiler_Internal::BuildDefineList(shader.GetKey(), environment);

    const std::expected<ComPtr<IDxcResult>, std::string> compilationResult =
        CompileShaderFromBuffer(shader.GetKey(), shaderSourceBuffer, args, dxcDefines, compilerSettings);
    if (!compilationResult.has_value())
    {
        return std::unexpected(compilationResult.error());
    }

    ComPtr<IDxcBlob> shaderBytecode;
    const HRESULT objectResult =
        compilationResult.value()->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBytecode), nullptr);
    if (FAILED(objectResult))
    {
        return std::unexpected("Failed to obtain the shader blob after compilation.");
    }

    // Store shader bytecode blob inside the Shader.
    std::vector<byte> finalShaderBlob;
    finalShaderBlob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(finalShaderBlob.data(), shaderBytecode->GetBufferPointer(), finalShaderBlob.size() * sizeof(u8));

    // Shader reflection data.
    std::optional<ShaderReflection> reflection;
    if (ShaderUtil::CanReflectShaderType(shader.GetKey().type))
    {
        if (compilerSettings.target == CompilationTarget::DXIL)
        {
#if VEX_DX12
            reflection = GetDxcReflection(*compilationResult);
#else
            VEX_LOG(Warning, "Unable to get DXIL reflection when compiling without DX12 graphics API.");
#endif
        }
        else if (compilerSettings.target == CompilationTarget::SPIRV)
        {
#if VEX_VULKAN
            reflection = GetSpirvReflection(finalShaderBlob);
#else
            VEX_LOG(Warning, "Unable to get spirv reflection when compiling without Vulkan graphics API.");
#endif
        }
    }

    return ShaderCompilationResult{ shaderHash.value(), finalShaderBlob, reflection };
}

std::expected<ComPtr<IDxcResult>, std::string> DXCCompiler::CompileShaderFromBuffer(
    const ShaderKey& key,
    const DxcBuffer& shaderSource,
    const std::vector<std::wstring>& args,
    const std::vector<std::pair<std::wstring, std::wstring>>& defines,
    const ShaderCompilerSettings& compilerSettings) const
{
    std::wstring entryPoint;
    // RT Shaders are compiled differently to other shader types, the entry point should be null.
    if (!IsRayTracingShader(key.type))
    {
        entryPoint = StringToWString(key.entryPoint);
    }

    std::vector<LPCWSTR> wstrArgs;
    std::ranges::transform(args, std::back_inserter(wstrArgs), [](const std::wstring& str) { return str.data(); });

    // Convert to DxcDefine type.
    std::vector<DxcDefine> dxcDefines;
    dxcDefines.reserve(defines.size());
    std::ranges::transform(defines,
                           std::back_inserter(dxcDefines),
                           [](const std::pair<std::wstring, std::wstring>& d)
                           { return DxcDefine{ .Name = d.first.c_str(), .Value = d.second.c_str() }; });

    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (const HRESULT hr =
            utils->BuildArguments(StringToWString(key.filepath).c_str(),
                                  entryPoint.empty() ? nullptr : entryPoint.c_str(),
                                  DXCCompiler_Internal::GetTargetFromShaderType(key.type, compilerSettings).c_str(),
                                  wstrArgs.data(),
                                  static_cast<u32>(args.size()),
                                  dxcDefines.data(),
                                  static_cast<u32>(dxcDefines.size()),
                                  &compilerArgs);
        FAILED(hr))
    {
        return std::unexpected(std::format("Failed to build shader compilation arguments from file: {}", key.filepath));
    }

    ComPtr<IDxcResult> shaderCompilationResults;
    const HRESULT hrCompile = compiler->Compile(&shaderSource,
                                                compilerArgs->GetArguments(),
                                                compilerArgs->GetCount(),
                                                defaultIncludeHandler.operator->(),
                                                IID_PPV_ARGS(&shaderCompilationResults));

    ComPtr<IDxcBlobUtf8> errors = nullptr;
    if (const HRESULT hrError = shaderCompilationResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        FAILED(hrError))
    {
        return std::unexpected("Failed to compile shader due to unknown reasons, the DXC compilation error "
                               "was unable to be obtained.");
    }
    if (errors != nullptr && errors->GetStringLength() != 0)
    {
        return std::unexpected(std::format("{}", errors->GetStringPointer()));
    }
    if (FAILED(hrCompile))
    {
        return std::unexpected("Failed to compile shader due to unknown reasons, the DXC compilation error "
                               "was unable to be obtained.");
    }

    return shaderCompilationResults;
}

} // namespace vex::sc