#include "ShaderCompiler.h"

#include <istream>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/Shader.h>
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

struct ShaderParser
{
    struct GlobalResource
    {
        std::string type;
        std::string name;
    };

    struct LocalConstants
    {
        std::string type;
        std::string name;
    };

    struct ShaderBlock
    {
        std::string fullShaderBlock;
        std::vector<GlobalResource> globalResources;
        std::optional<LocalConstants> localConstants;

        // Position information for faster replacement.
        size_t blockStartPos;
        size_t blockEndPos;
        size_t blockLength;
    };

    enum ParseError
    {
        NoVexShaderFound,
        MultipleVexShaders,
        MultipleLocalConstants,
        EmptyType,
        UsingHLSLPrimitiveType,
        EmptyName,
        InvalidIdentifier,
    };

    // FYI: The following regex were generated with ChatGPT, there might be some weird edge cases we're not aware of.
    static const std::regex ShaderBlockRegex()
    {
        // Matches VEX_SHADER followed by { ... } with proper brace matching
        return std::regex{ R"(VEX_SHADER\s*\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\})",
                           /*std::regex_constants::multiline | */ std::regex_constants::ECMAScript };
    }
    static const std::regex GlobalResourceRegex()
    {
        // Captures: VEX_GLOBAL_RESOURCE(Type, Name);
        return std::regex{ R"(VEX_GLOBAL_RESOURCE\s*\(\s*([^,]+?)\s*,\s*([^)\s][^)]*?)\s*\)\s*;?)" };
    }
    static const std::regex LocalConstantsRegex()
    {
        // Captures: VEX_LOCAL_RESOURCE(Type, Name);
        return std::regex{ R"(VEX_LOCAL_CONSTANTS\s*\(\s*([^,\s][^,]*?)\s*,\s*([^)\s][^)]*?)\s*\)\s*;?)" };
    }

    static std::expected<ShaderBlock, ParseError> ParseShader(const std::string& shaderCode)
    {
        ShaderBlock result;

        // Step 1: Find the VEX_SHADER block
        std::smatch shaderMatch;
        auto shaderRegex = ShaderBlockRegex();

        std::vector<std::smatch> shaderMatches;
        std::sregex_iterator shaderIter(shaderCode.begin(), shaderCode.end(), shaderRegex);
        std::sregex_iterator end;

        for (; shaderIter != end; ++shaderIter)
        {
            shaderMatches.push_back(*shaderIter);
        }

        if (shaderMatches.empty())
        {
            return std::unexpected(NoVexShaderFound);
        }

        if (shaderMatches.size() > 1)
        {
            return std::unexpected(MultipleVexShaders);
        }

        const std::smatch& match = shaderMatches[0];
        result.fullShaderBlock = match[0].str(); // Full match including VEX_SHADER{...}
        result.blockStartPos = match[0].first - shaderCode.begin();
        result.blockEndPos = match[0].second - shaderCode.begin();
        result.blockLength = result.blockEndPos - result.blockStartPos;
        std::string vexShaderBlockContent = match[1].str(); // Content inside braces

        // Step 2: Parse VEX_GLOBAL_RESOURCE declarations
        auto globalRegex = GlobalResourceRegex();
        std::sregex_iterator globalIter(vexShaderBlockContent.begin(), vexShaderBlockContent.end(), globalRegex);
        for (; globalIter != end; ++globalIter)
        {
            const auto& globalMatch = *globalIter;
            GlobalResource resource;
            resource.type = Trim(globalMatch[1].str());
            resource.name = Trim(globalMatch[2].str());
            result.globalResources.push_back(resource);
        }

        // Step 3: Parse VEX_LOCAL_CONSTANTS declaration
        auto localRegex = LocalConstantsRegex();
        std::sregex_iterator localIter(vexShaderBlockContent.begin(), vexShaderBlockContent.end(), localRegex);

        for (; localIter != end; ++localIter)
        {
            // Second pass means we've found more than one invocation of LocalConstants which is invalid.
            if (result.localConstants.has_value())
            {
                return std::unexpected(MultipleLocalConstants);
            }

            const auto& localMatch = *localIter;
            LocalConstants constants;
            constants.type = Trim(localMatch[1].str());
            constants.name = Trim(localMatch[2].str());
            result.localConstants = constants;
        }

        // Step 4: Additional validation for local constants if present
        if (result.localConstants.has_value())
        {
            const auto& localConst = result.localConstants.value();

            if (localConst.type.empty())
            {
                return std::unexpected(EmptyType);
            }

            if (IsPrimitiveType(localConst.type))
            {
                return std::unexpected(UsingHLSLPrimitiveType);
            }

            if (localConst.name.empty())
            {
                return std::unexpected(EmptyName);
            }

            if (!IsValidIdentifier(localConst.name))
            {
                return std::unexpected(InvalidIdentifier);
            }
        }

        return result;
    }

    static std::string ReplaceVexShaderBlock(const std::string& originalCode,
                                             const ShaderBlock& parsedBlock,
                                             const std::string& replacementBlock)
    {
        // Pre-calculate final size to avoid reallocations
        const size_t newSize = originalCode.length() - parsedBlock.blockLength + replacementBlock.length();

        std::string result;
        result.reserve(newSize);

        // Copy parts: before block + replacement + after block
        result.append(originalCode, 0, parsedBlock.blockStartPos);
        result.append(replacementBlock);
        result.append(originalCode, parsedBlock.blockEndPos, std::string::npos);

        return result;
    }

private:
    static bool IsValidIdentifier(const std::string& str)
    {
        // Regex to validate C++ identifiers
        static const std::regex ValidIdentifierRegex{ R"(^[a-zA-Z_][a-zA-Z0-9_]*$)" };
        return std::regex_match(str, ValidIdentifierRegex);
    }

    static std::string Trim(const std::string& str)
    {
        const auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";

        const auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    // Check if a type is a built-in HLSL primitive type
    static bool IsPrimitiveType(std::string_view typeName)
    {
        // Common HLSL primitive types that don't allow for forward declarations
        // clang-format off
        static constexpr auto primitives =
        {
            // Scalar types
            "bool", "int", "uint", "float", "double",
            // Vector types
            "float2", "float3", "float4", 
            "int2", "int3", "int4",
            "uint2", "uint3", "uint4",
            "bool2", "bool3", "bool4",
            "double2", "double3", "double4",
            // Matrix types
            "float2x2", "float3x3", "float4x4",
            "float2x3", "float2x4", "float3x2", "float3x4", "float4x2", "float4x3",
            "int2x2", "int3x3", "int4x4",
            "uint2x2", "uint3x3", "uint4x4",
            "bool2x2", "bool3x3", "bool4x4",
            "double2x2", "double3x3", "double4x4",
            // Alternative matrix syntax
            "matrix", "vector"
        };
        // clang-format on

        // Check exact match first
        if (auto it = std::find(primitives.begin(), primitives.end(), typeName); it != primitives.end())
        {
            return true;
        }

        // Check for templated types like matrix<float, 4, 4>
        if (typeName.find("matrix<") == 0 || typeName.find("vector<") == 0)
        {
            return true;
        }

        return false;
    }
};

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

ComPtr<IDxcResult> ShaderCompiler::GetPreprocessedShader(const Shader& shader,
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

std::optional<std::size_t> ShaderCompiler::GetShaderHash(const Shader& shader) const
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

std::expected<void, std::string> ShaderCompiler::CompileShader(Shader& shader,
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

    // Manually read the user shader file.
    std::stringstream buffer;
    {
        std::ifstream shaderFile{ shader.key.path.c_str() };
        buffer << shaderFile.rdbuf();
    }
    std::string shaderFileStr = buffer.str();

    using ShaderCompiler_Internal::ShaderParser;
    // We insert the local constants on the vulkan side by replacing the first declaration of VEX_LOCAL_CONSTANTS,
    // however this macro should only appear once and has some other constraints. This parser validates the usage of
    // VEX_LOCAL_CONSTANTS in the user shader code.
    auto res = ShaderParser::ParseShader(shaderFileStr);
    if (!res.has_value())
    {
        switch (res.error())
        {
        case ShaderParser::NoVexShaderFound:
            return std::unexpected("ShaderCompiler: When parsing for VEX_SHADER, no occurrences were found. Please "
                                   "include a VEX_SHADER block for shader code-gen.");
        case ShaderParser::MultipleVexShaders:
            return std::unexpected("ShaderCompiler: When parsing for VEX_SHADER, multiple VEX_SHADER blocks were "
                                   "found, only one occurrence of this block is allowed.");
        case ShaderParser::MultipleLocalConstants:
            return std::unexpected(
                "ShaderCompiler: When parsing for local constants, multiple uses of the VEX_LOCAL_CONSTANTS macro were "
                "found. Make sure to only use it once (including all your local constants in it).");
        case ShaderParser::EmptyType:
            return std::unexpected(
                "ShaderCompiler: When parsing for local constants, a usage of VEX_LOCAL_CONSTANTS was detected with an "
                "empty type. Make sure to fill in the type of VEX_LOCAL_CONSTANTS(type, name)!");
        case ShaderParser::UsingHLSLPrimitiveType:
            return std::unexpected(
                "ShaderCompiler: Your VEX_LOCAL_CONSTANTS type cannot be a direct primitive type, instead you must "
                "wrap it inside a custom struct. Eg: 'VEX_LOCAL_CONSTANTS(float2, myFloat)' is not valid, but 'struct "
                "MyFloatS { float2 val }; VEX_LOCAL_CONSTANTS(MyFloatS, myFloat)' is valid.");
        case ShaderParser::EmptyName:
            return std::unexpected(
                "ShaderCompiler: When parsing for local constants, a usage of VEX_LOCAL_CONSTANTS was detected with an "
                "empty variable name. Make sure to fill in the name of VEX_LOCAL_CONSTANTS(type, name)!");
        case ShaderParser::InvalidIdentifier:
            return std::unexpected("ShaderCompiler: When parsing for local constants, a usage of VEX_LOCAL_CONSTANTS "
                                   "was detected with an invalid name (must be a valid C++ identifier).");
        }
    }
    else
    {
        const ShaderParser::ShaderBlock& shaderBlockInfo = res.value();
        // Generate structs.
        std::string codeGen = std::string(ShaderGenBindingMacros);
        codeGen.append("struct Vex_GeneratedGlobalResources\n{\n");
        for (const ShaderParser::GlobalResource& resource : shaderBlockInfo.globalResources)
        {
            codeGen.append(std::format("\t uint {}_BindlessIndex;\n", resource.name));
        }
// Generate ConstantBuffers.
#if VEX_VULKAN
        codeGen.append("};\n"
                       "struct Vex_GeneratedCombinedResources\n"
                       "{\n"
                       "\t uint GlobalResourcesBindlessIndex;\n");
        if (shaderBlockInfo.localConstants.has_value())
        {
            codeGen.append(std::format("\t{} UserData;\n", shaderBlockInfo.localConstants->type));
        }
        codeGen.append(
            "};\n"
            "[[vk::push_constant]] ConstantBuffer<Vex_GeneratedCombinedResources> Vex_GeneratedCombinedResourcesCB;\n"
            "static ConstantBuffer<Vex_GeneratedGlobalResources> Vex_GeneratedGlobalResourcesCB = "
            "ResourceDescriptorHeap[Vex_GeneratedCombinedResourcesCB.GlobalResourcesBindlessIndex];\n");
        // Insert the macro to make the local binding transparent for the user.
        if (shaderBlockInfo.localConstants.has_value())
        {
            codeGen.append(std::format("#define {} (Vex_GeneratedCombinedResourcesCB.UserData)\n",
                                       shaderBlockInfo.localConstants->name));
        }
#elif VEX_DX12
        codeGen.append(std::format(
            "}};\nConstantBuffer<Vex_GeneratedGlobalResources> Vex_GeneratedGlobalResourcesCB : register(b0);\n"));
        // Insert the macro to make the local binding transparent for the user.
        if (shaderBlockInfo.localConstants.has_value())
        {
            codeGen.append(std::format("ConstantBuffer<{}> {} : register(b1);\n",
                                       shaderBlockInfo.localConstants->type,
                                       shaderBlockInfo.localConstants->name));
        }
#endif
        // Insert static declarations for global resources.
        for (const ShaderParser::GlobalResource& resource : shaderBlockInfo.globalResources)
        {
            codeGen.append(std::format(
                "static {0} {1} = ResourceDescriptorHeap[Vex_GeneratedGlobalResourcesCB.{1}_BindlessIndex];\n",
                resource.type,
                resource.name));
        }

        // Auto-generate shader static sampler bindings.
        for (u32 i = 0; i < resourceContext.samplers.size(); ++i)
        {
            const TextureSampler& sampler = resourceContext.samplers[i];
            codeGen.append(std::format("SamplerState {} : register(s{}, space0);\n", sampler.name, i));
        }

        // Replace the VEX_SHADER block with our codegen.
        shaderFileStr = ShaderParser::ReplaceVexShaderBlock(shaderFileStr, shaderBlockInfo, codeGen);
    }

#if !VEX_SHIPPING
    VEX_LOG(Info, "Shader {}\nFile dump:\n{}", shader.key, shaderFileStr);
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

    shaderDefines.emplace_back(L"VEX_DEBUG", std::to_wstring(VEX_DEBUG));
    shaderDefines.emplace_back(L"VEX_DEVELOPMENT", std::to_wstring(VEX_DEVELOPMENT));
    shaderDefines.emplace_back(L"VEX_SHIPPING", std::to_wstring(VEX_SHIPPING));
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
    shader.blob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(shader.blob.data(), shaderBytecode->GetBufferPointer(), shader.blob.size() * sizeof(u8));

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

    shader.version++;
    shader.isDirty = false;

    return {};
}

Shader* ShaderCompiler::GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext)
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

    return shaderPtr;
}

std::pair<bool, std::size_t> ShaderCompiler::IsShaderStale(const Shader& shader) const
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
    for (auto& shader : shaderCache | std::views::values)
    {
        shader.MarkDirty();
        shader.isErrored = false;
    }

    VEX_LOG(Info, "Marked all shaders for recompilation...");
}

void ShaderCompiler::MarkAllStaleShadersDirty()
{
    u32 numStaleShaders = 0;
    for (auto& shader : shaderCache | std::views::values)
    {
        if (auto [isShaderStale, newShaderHash] = IsShaderStale(shader); isShaderStale || shader.isErrored)
        {
            shader.hash = newShaderHash;
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