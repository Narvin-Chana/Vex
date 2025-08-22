#include "ShaderCompiler.h"

#include <regex>
#include <string>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/Shaders/CompilerInterface.h>
#include <Vex/Shaders/DXCImpl.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderGen.h>
#include <Vex/Shaders/ShaderResourceContext.h>
#include <Vex/TextureSampler.h>

namespace vex
{

namespace ShaderCompiler_Internal
{

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

    // Can't use string_view as <regex> requires a non-const bidirectional iterator...
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

ShaderCompiler::ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , compilerSettings(compilerSettings)
{
#if VEX_SHIPPING
    // Force disable shader debugging in shipping.
    this->compilerSettings.enableShaderDebugging = false;
#endif

    switch (compilerSettings.compilerBackend)
    {
    case ShaderCompilerBackend::DXC:
    {
        compilerImpl = MakeUnique<DXCCompilerImpl>();
        break;
    }
    }
}

ShaderCompiler::~ShaderCompiler() = default;

std::expected<std::string, std::string> ShaderCompiler::ProcessShaderCodeGen(
    const std::string& shaderFileStr, const ShaderResourceContext& resourceContext)
{
    using ShaderCompiler_Internal::ShaderParser;
    // We insert the local constants on the Vulkan side by replacing the first declaration of VEX_LOCAL_CONSTANTS,
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

    ShaderParser::ShaderBlock& shaderBlockInfo = res.value();
    // Generate structs.
    std::string codeGen = std::string(ShaderGenBindingMacros);
    codeGen.append("struct Vex_GeneratedGlobalResources\n{\n");
    // Sort from A-Z to ensure constant bindings matches the order of ResourceLayout's bindless buffer.
    std::sort(shaderBlockInfo.globalResources.begin(),
              shaderBlockInfo.globalResources.end(),
              [](const ShaderParser::GlobalResource& lh, const ShaderParser::GlobalResource& rh)
              { return lh.name < rh.name; });
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
        codeGen.append(
            std::format("static {0} {1} = ResourceDescriptorHeap[Vex_GeneratedGlobalResourcesCB.{1}_BindlessIndex];\n",
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
    return ShaderParser::ReplaceVexShaderBlock(shaderFileStr, shaderBlockInfo, codeGen);
}

ShaderEnvironment ShaderCompiler::CreateShaderEnvironment()
{
    ShaderEnvironment env;
    env.defines.emplace_back(L"VEX_DEBUG", std::to_wstring(VEX_DEBUG));
    env.defines.emplace_back(L"VEX_DEVELOPMENT", std::to_wstring(VEX_DEVELOPMENT));
    env.defines.emplace_back(L"VEX_SHIPPING", std::to_wstring(VEX_SHIPPING));
    env.defines.emplace_back(L"VEX_RAYTRACING",
                             GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing) ? L"1" : L"0");

    rhi->ModifyShaderCompilerEnvironment(compilerSettings.compilerBackend, env);

    return env;
}

std::expected<void, std::string> ShaderCompiler::CompileShader(Shader& shader,
                                                               const ShaderResourceContext& resourceContext)
{
    if (!GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::BindlessResources))
    {
        VEX_LOG(Fatal, "Vex requires BindlessResources in order to bind global resources.");
    }

    // Generate the hash if this is the first time we've compiled this shader.
    if (shader.version == 0)
    {
        std::optional<std::size_t> newHash = compilerImpl->GetShaderHash(shader, additionalIncludeDirectories);
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

    // Shader code-gen, should work in any compilation backend, if not I'll move it into the impls later on.
    {
        auto res = ProcessShaderCodeGen(shaderFileStr, resourceContext);
        if (!res.has_value())
        {
            return std::unexpected(res.error());
        }
        shaderFileStr = std::move(res.value());
    }

#if !VEX_SHIPPING
    VEX_LOG(Verbose, "Shader {}\nFile dump:\n{}", shader.key, shaderFileStr);
#endif

    ShaderEnvironment env = CreateShaderEnvironment();

    auto res = compilerImpl->CompileShader(shaderFileStr, env, additionalIncludeDirectories, compilerSettings, shader);
    if (!res.has_value())
    {
        return std::unexpected(res.error());
    }
    std::vector<u8>& shaderBytecode = res.value();

    // Store shader bytecode blob inside the Shader.
    shader.blob = std::move(shaderBytecode);

    shader.version++;
    shader.isDirty = false;

    return {};
}

NonNullPtr<Shader> ShaderCompiler::GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext)
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

    return NonNullPtr(shaderPtr);
}

std::pair<bool, std::size_t> ShaderCompiler::IsShaderStale(const Shader& shader) const
{
    if (!std::filesystem::exists(shader.key.path))
    {
        VEX_LOG(Fatal, "Unable to parse a shader file which no longer exists: {}.", shader.key);
        return { false, shader.hash };
    }

    std::optional<std::size_t> newHash = compilerImpl->GetShaderHash(shader, additionalIncludeDirectories);
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