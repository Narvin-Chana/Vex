#include "ShaderCompiler.h"

#include <ranges>

#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIShader.h>

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
    // TODO(https://trello.com/c/JjGISzqs): Make this use the FeatureChecker, which would allow us to use the highest
    // target supported.
    if (type == ShaderType::VertexShader)
    {
        return L"vs_6_6";
    }
    if (type == ShaderType::PixelShader)
    {
        return L"ps_6_6";
    }
    if (type == ShaderType::ComputeShader)
    {
        return L"cs_6_6";
    }
    return L"";
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

ShaderCache::ShaderCache(RHI* rhi, bool enableShaderDebugging)
    : rhi(rhi)
    , debugShaders(enableShaderDebugging)
{
}

ShaderCache::~ShaderCache() = default;

thread_local CompilerUtil ShaderCache::GCompilerUtil;

ComPtr<IDxcResult> ShaderCache::GetPreprocessedShader(const RHIShader& shader,
                                                      const ComPtr<IDxcBlobEncoding>& shaderBlobUTF8) const
{
    std::vector<LPCWSTR> args = { L"-P" }; // -P gives us the preprocessor output of the shader.
    FillInAdditionalIncludeDirectories(args);

    DxcBuffer buffer = { .Ptr = shaderBlobUTF8->GetBufferPointer(),
                         .Size = shaderBlobUTF8->GetBufferSize(),
                         .Encoding = CP_UTF8 };

    ComPtr<IDxcResult> result;
    if (HRESULT hr = GCompilerUtil.compiler
                         ->Compile(&buffer, args.data(), static_cast<u32>(args.size()), nullptr, IID_PPV_ARGS(&result));
        FAILED(hr))
    {
        return nullptr;
    }

    return result;
}

void ShaderCache::FillInAdditionalIncludeDirectories(std::vector<LPCWSTR>& args) const
{
    for (auto& additionalIncludeFolder : additionalIncludeDirectories)
    {
        args.insert(args.end(), { L"-I", StringToWString(additionalIncludeFolder.string()).c_str() });
    }
}

std::optional<std::size_t> ShaderCache::GetShaderHash(const RHIShader& shader) const
{
    ComPtr<IDxcBlobEncoding> shaderBlobUTF8;
    u32 codePage = CP_UTF8;
    if (HRESULT hr = GCompilerUtil.utils->LoadFile(shader.key.path.wstring().c_str(), &codePage, &shaderBlobUTF8);
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

std::expected<void, std::string> ShaderCache::CompileShader(RHIShader& shader)
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

    ComPtr<IDxcBlobEncoding> shaderBlob;
    if (HRESULT hr = GCompilerUtil.utils->LoadFile(shader.key.path.wstring().c_str(), nullptr, &shaderBlob); FAILED(hr))
    {
        return std::unexpected("Failed to load shader from filesystem.");
    }

    DxcBuffer shaderSource = {};
    shaderSource.Ptr = shaderBlob->GetBufferPointer();
    shaderSource.Size = shaderBlob->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    std::vector<LPCWSTR> args;

    rhi->ModifyShaderCompilerEnvironment(args, shader.key.defines);

    if (debugShaders)
    {
        args.insert(args.end(),
                    {
                        DXC_ARG_DEBUG,
                        DXC_ARG_WARNINGS_ARE_ERRORS,
                        DXC_ARG_DEBUG_NAME_FOR_SOURCE,
                        L"-Qembed_debug",
                    });
    }

    FillInAdditionalIncludeDirectories(args);

    std::vector<DxcDefine> defines = ShaderCompiler_Internal::ConvertDefinesToDxcDefine(shader.key.defines);
    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (HRESULT hr = GCompilerUtil.utils->BuildArguments(
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
    HRESULT compilationHR = GCompilerUtil.compiler->Compile(&shaderSource,
                                                            compilerArgs->GetArguments(),
                                                            compilerArgs->GetCount(),
                                                            GCompilerUtil.defaultIncludeHandler.operator->(),
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

    // Reflection blob, to be implemented later on.
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

RHIShader* ShaderCache::GetShader(const ShaderKey& key)
{
    RHIShader* shaderPtr;
    if (shaderCache.contains(key))
    {
        shaderPtr = shaderCache[key].get();
    }
    else
    {
        UniqueHandle<RHIShader> shader = rhi->CreateShader(key);
        shaderPtr = shader.get();
        shaderCache[key] = std::move(shader);
    }

    if (shaderPtr->NeedsRecompile())
    {
        auto result = CompileShader(*shaderPtr);
        // Currently this is just spat out to cout, however a future task (separate MR) will be to determine how to pass
        // error info/retry info to the client user programatically.
        //
        // @alemarbre if you have any ideas I'd be interested.
        // There's always the option of some sort of ShaderError callback that the user can subscribe to.
        //
        // Otherwise we could add the concept of log categories to the logger (my least preferred option) but it seems
        // to be quite a bit more convoluted and requires more needless effort imo (we're a graphics lib, not a logging
        // lib).
        //
        // The more C-style option is a function you can call periodically - a GetShaderCompilationErrors() that fills
        // in a string vector with the errors since the last compilation attempt.
        if (!result.has_value())
        {
            VEX_LOG(Error, "Failed to compile shader:\n\t- {}:\n\t- Reason: {}", shaderPtr->key, result.error());
        }
    }

    return shaderPtr;
}

std::pair<bool, std::size_t> ShaderCache::IsShaderStale(const RHIShader& shader) const
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

void ShaderCache::MarkShaderDirty(const ShaderKey& key)
{
    if (!shaderCache.contains(key))
    {
        VEX_LOG(Error,
                "The shader key passed did not yield any valid shaders in the shader cache (key {}). Unable to mark it "
                "as dirty.",
                key);
        return;
    }

    shaderCache[key]->MarkDirty();
}

void ShaderCache::MarkAllShadersDirty()
{
    for (auto& shader : shaderCache | std::views::values)
    {
        shader->MarkDirty();
    }
}

void ShaderCache::MarkAllStaleShadersDirty()
{
    u32 numStaleShaders = 0;
    for (auto& shader : shaderCache | std::views::values)
    {
        if (auto [isShaderStale, newShaderHash] = IsShaderStale(*shader); isShaderStale)
        {
            shader->hash = newShaderHash;
            shader->MarkDirty();
            numStaleShaders++;
        }
    }

    if (numStaleShaders > 0)
    {
        VEX_LOG(Info, "Marked {} shaders for recompilation...", numStaleShaders);
    }
}

} // namespace vex