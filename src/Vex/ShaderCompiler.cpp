#include "ShaderCompiler.h"

#include <istream>
#include <ranges>
#include <string>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIShader.h>
#include <Vex/ShaderGen.h>
#include <Vex/ShaderResourceContext.h>
#include <Vex/TextureSampler.h>

namespace vex
{

namespace ShaderCompiler_Internal
{

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

static std::wstring GetTargetFromShaderType(ShaderType type)
{
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();

    std::wstring highestSupportedShaderModel =
        StringToWString(std::string(magic_enum::enum_name(featureChecker->GetShaderModel())));

    // First char changes depending on shader type.
    switch (type)
    {
    case ShaderType::VertexShader:
        highestSupportedShaderModel[0] = L'v';
        break;
    case ShaderType::PixelShader:
        highestSupportedShaderModel[0] = L'p';
        break;
    case ShaderType::ComputeShader:
        highestSupportedShaderModel[0] = L'c';
        break;
    default:
        VEX_LOG(Fatal, "Unsupported shader type for the Vex ShaderCompiler.");
        break;
    }

    // Second character is always 's'.
    highestSupportedShaderModel[1] = L's';

    return highestSupportedShaderModel;
}

} // namespace ShaderCompiler_Internal

CompilerUtil::CompilerUtil()
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

CompilerUtil::~CompilerUtil() = default;

ShaderCompiler::ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , compilerSettings(compilerSettings)
{
#if VEX_SHIPPING
    // Force disable shader debugging in shipping.
    this->compilerSettings.enableShaderDebugging = false;
#endif
}

ShaderCompiler::~ShaderCompiler() = default;

ComPtr<IDxcResult> ShaderCompiler::GetPreprocessedShader(const RHIShader& shader,
                                                         const ComPtr<IDxcBlobEncoding>& shaderBlobUTF8) const
{
    std::vector<LPCWSTR> args = { L"-P" }; // -P gives us the preprocessor output of the shader.
    FillInAdditionalIncludeDirectories(args);

    DxcBuffer buffer = { .Ptr = shaderBlobUTF8->GetBufferPointer(),
                         .Size = shaderBlobUTF8->GetBufferSize(),
                         .Encoding = CP_UTF8 };

    ComPtr<IDxcResult> result;
    if (HRESULT hr = GetCompilerUtil().compiler->Compile(&buffer,
                                                         args.data(),
                                                         static_cast<u32>(args.size()),
                                                         nullptr,
                                                         IID_PPV_ARGS(&result));
        FAILED(hr))
    {
        return nullptr;
    }

    return result;
}

void ShaderCompiler::FillInAdditionalIncludeDirectories(std::vector<LPCWSTR>& args) const
{
    for (auto& additionalIncludeFolder : additionalIncludeDirectories)
    {
        args.insert(args.end(), { L"-I", StringToWString(additionalIncludeFolder.string()).c_str() });
    }
}

CompilerUtil& ShaderCompiler::GetCompilerUtil()
{
    thread_local CompilerUtil GCompilerUtil;
    return GCompilerUtil;
}

std::optional<std::size_t> ShaderCompiler::GetShaderHash(const RHIShader& shader) const
{
    ComPtr<IDxcBlobEncoding> shaderBlobUTF8;
    u32 codePage = CP_UTF8;
    if (HRESULT hr = GetCompilerUtil().utils->LoadFile(shader.key.path.wstring().c_str(), &codePage, &shaderBlobUTF8);
        FAILED(hr))
    {
        VEX_LOG(Error, "Unable to get shader hash, failed to load shader from filepath: {}", shader.key.path.string());
        return std::nullopt;
    }

    if (ComPtr<IDxcResult> preprocessingResult = GetPreprocessedShader(shader, shaderBlobUTF8))
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

std::expected<void, std::string> ShaderCompiler::CompileShader(RHIShader& shader,
                                                               const ShaderResourceContext& resourceContext)
{
    // Generate the hash if this is the first time we've compiled this shader.
    if (shader.version == 0)
    {
        std::optional<std::size_t> newHash = GetShaderHash(shader);
        if (!newHash)
        {
            return std::unexpected("Failed to generate shader hash.");
        }
        shader.hash = *newHash;
    }

    std::stringstream buffer;
    {
        std::ifstream shaderFile{ shader.key.path.c_str() };
        buffer << shaderFile.rdbuf();
    }

    std::string shaderFileStr = ShaderGenGeneralMacros;

    // Auto-generate shader static sampler bindings.
    shaderFileStr.append("// SAMPLERS -------------------------\n");
    for (u32 i = 0; i < resourceContext.samplers.size(); ++i)
    {
        const TextureSampler& sampler = resourceContext.samplers[i];
        shaderFileStr.append(std::format("SamplerState {} : register(s{}, space0);\n", sampler.name, i));
    }

    // Auto-generate shader constants bindings.
    shaderFileStr.append("// GENERATED CONSTANTS -------------------------\n");
    shaderFileStr.append("struct zzzZZZ___GeneratedConstants\n{");
    for (std::string& name : resourceContext.GenerateShaderBindings())
    {
        // Remove spaces, we suppose that the user will not use any tabs or other cursed characters.
        std::replace(name.begin(), name.end(), ' ', '_');
        shaderFileStr.append(std::format("uint {}_bindlessIndex;\n", name));
    }
    // For now we suppose that the register b0 is used for the generated constants buffer (since local constants aren't
    // yet supported).
    shaderFileStr.append(
        "};\nVEX_LOCAL_CONSTANT ConstantBuffer<zzzZZZ___GeneratedConstants> zzzZZZ___GeneratedConstantsCB: "
        "register(b0);\n");

    // VEX_GLOBAL_RESOURCE and VEX_RESOURCE is how users will access resources, include macros that generate these.
    shaderFileStr.append(ShaderGenBindingMacros);

    // Append the actual shader file contents to the str.
    shaderFileStr.append(buffer.str());

#if !VEX_SHIPPING
    VEX_LOG(Verbose, "Shader {} file dump: {}", shader.key, shaderFileStr);
#endif

    ComPtr<IDxcBlobEncoding> shaderBlob;
    if (HRESULT hr = GetCompilerUtil().utils->CreateBlobFromPinned(shaderFileStr.c_str(),
                                                                   shaderFileStr.size(),
                                                                   CP_UTF8,
                                                                   &shaderBlob);
        FAILED(hr))
    {
        return std::unexpected("Failed to load shader from filesystem.");
    }

    DxcBuffer shaderSource = {};
    shaderSource.Ptr = shaderBlob->GetBufferPointer();
    shaderSource.Size = shaderBlob->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    std::vector<LPCWSTR> args;

    std::vector<ShaderDefine> shaderDefines = shader.key.defines;

    rhi->ModifyShaderCompilerEnvironment(args, shaderDefines);

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
        args.insert(args.end(), HLSL202xFlags);
    }

    FillInAdditionalIncludeDirectories(args);

    std::vector<DxcDefine> defines = ShaderCompiler_Internal::ConvertDefinesToDxcDefine(shaderDefines);
    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (HRESULT hr = GetCompilerUtil().utils->BuildArguments(
            shader.key.path.wstring().c_str(),
            StringToWString(shader.key.entryPoint).c_str(),
            ShaderCompiler_Internal::GetTargetFromShaderType(shader.key.type).c_str(),
            args.data(),
            static_cast<u32>(args.size()),
            defines.data(),
            static_cast<u32>(defines.size()),
            &compilerArgs);
        FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to build shader compilation arguments from file: {}", shader.key.path.string()));
    }

    ComPtr<IDxcResult> shaderCompilationResults;
    HRESULT compilationHR = GetCompilerUtil().compiler->Compile(&shaderSource,
                                                                compilerArgs->GetArguments(),
                                                                compilerArgs->GetCount(),
                                                                GetCompilerUtil().defaultIncludeHandler.operator->(),
                                                                IID_PPV_ARGS(&shaderCompilationResults));

    ComPtr<IDxcBlobUtf8> errors = nullptr;
    if (HRESULT hrError = shaderCompilationResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        FAILED(hrError))
    {
        return std::unexpected(
            "Failed to compile shader due to unknown reasons, the DXC compilation error was unable to be obtained.");
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

    // Store shader bytecode blob inside the RHIShader.
    shader.blob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(shader.blob.data(), shaderBytecode->GetBufferPointer(), shader.blob.size() * sizeof(u8));

    // TODO: Reflection blob, to be implemented later on.
    {
        // ComPtr<IDxcBlob> reflectionBlob;
        // HRESULT reflectionResult =
        //     shaderCompilationResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);
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

    shader.version++;
    shader.isDirty = false;

    return {};
}

RHIShader* ShaderCompiler::GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext)
{
    RHIShader* shaderPtr;
    if (shaderCompiler.contains(key))
    {
        shaderPtr = shaderCompiler[key].get();
    }
    else
    {
        UniqueHandle<RHIShader> shader = rhi->CreateShader(key);
        shaderPtr = shader.get();
        shaderCompiler[key] = std::move(shader);
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

    return shaderPtr;
}

std::pair<bool, std::size_t> ShaderCompiler::IsShaderStale(const RHIShader& shader) const
{
    if (!std::filesystem::exists(shader.key.path))
    {
        VEX_LOG(Fatal, "Unable to parse a shader file which no longer exists: {}.", shader.key);
        return { false, shader.hash };
    }

    std::optional<std::size_t> newHash = GetShaderHash(shader);
    if (!newHash)
    {
        return { false, shader.hash };
    }

    bool isShaderStale = shader.hash != *newHash;
    return { isShaderStale, *newHash };
}

void ShaderCompiler::MarkShaderDirty(const ShaderKey& key)
{
    auto shader = shaderCompiler.find(key);
    if (shader == shaderCompiler.end())
    {
        VEX_LOG(Error,
                "The shader key passed did not yield any valid shaders in the shader cache (key {}). Unable to mark it "
                "as dirty.",
                key);
        return;
    }

    shader->second->MarkDirty();
    shader->second->isErrored = false;
}

void ShaderCompiler::MarkAllShadersDirty()
{
    for (auto& shader : shaderCompiler | std::views::values)
    {
        shader->MarkDirty();
        shader->isErrored = false;
    }

    VEX_LOG(Info, "Marked all shaders for recompilation...");
}

void ShaderCompiler::MarkAllStaleShadersDirty()
{
    u32 numStaleShaders = 0;
    for (auto& shader : shaderCompiler | std::views::values)
    {
        if (auto [isShaderStale, newShaderHash] = IsShaderStale(*shader); isShaderStale || shader->isErrored)
        {
            shader->hash = newShaderHash;
            shader->MarkDirty();
            shader->isErrored = false;
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
    // If user requests a recompilation (depends on the callback), remove the isErrored flag from all errored shaders.
    if (errorsCallback && errorsCallback(compilationErrors))
    {
        for (auto& [key, error] : compilationErrors)
        {
            VEX_ASSERT(shaderCompiler.contains(key), "A shader in compilationErrors was not found in the cache...");
            // The next time we attempt to use this shader, it will be recompiled.
            shaderCompiler[key]->isErrored = false;
        }
        compilationErrors.clear();
    }
}

} // namespace vex