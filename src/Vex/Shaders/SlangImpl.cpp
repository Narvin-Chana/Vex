#include "SlangImpl.h"

#include <cctype>

#include <magic_enum/magic_enum.hpp>

#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>

#if VEX_VULKAN
#include <Vulkan/VkFeatureChecker.h>
#endif
#include <regex>
#include <unordered_set>

#include <Vex/Logger.h>

namespace vex
{

namespace SlangCompilerImpl_Internal
{

static SlangStage GetSlangStageFromShaderType(ShaderType type)
{
    using enum ShaderType;
    using enum SlangStage;
    switch (type)
    {
    case VertexShader:
        return SLANG_STAGE_VERTEX;
    case PixelShader:
        return SLANG_STAGE_PIXEL;
    case ComputeShader:
        return SLANG_STAGE_COMPUTE;
    case RayGenerationShader:
        return SLANG_STAGE_RAY_GENERATION;
    case RayMissShader:
        return SLANG_STAGE_MISS;
    case RayClosestHitShader:
        return SLANG_STAGE_CLOSEST_HIT;
    case RayAnyHitShader:
        return SLANG_STAGE_ANY_HIT;
    case RayIntersectionShader:
        return SLANG_STAGE_INTERSECTION;
    case RayCallableShader:
        return SLANG_STAGE_CALLABLE;
    default:
        VEX_LOG(Fatal, "Invalid shader type: {}", magic_enum::enum_name(type));
    }
    std::unreachable();
}

} // namespace SlangCompilerImpl_Internal

SlangCompilerImpl::SlangCompilerImpl(std::vector<std::filesystem::path> incDirs)
    : CompilerBase(std::move(incDirs))
{
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();

    std::string highestSupportedShaderModel = std::string(magic_enum::enum_name(featureChecker->GetShaderModel()));

    slang::createGlobalSession(globalSession.writeRef());

    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

    // Add include directories
    std::vector<std::string> stringPaths;
    // std::filesystem::c_str() returns wchar on windows, slang takes in chars, so we must have two vectors for the
    // conversion (one to store the strings, one for the actual c_strs).
    std::vector<const char*> cstrPaths;
    FillInIncludeDirectories(stringPaths, cstrPaths, sessionDesc);

    // Configure target for both DX12 and Vulkan
#if VEX_DX12
    // s and m must be lower-case.
    highestSupportedShaderModel[0] = std::tolower(highestSupportedShaderModel[0]);
    highestSupportedShaderModel[1] = std::tolower(highestSupportedShaderModel[0]);
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = globalSession->findProfile(highestSupportedShaderModel.c_str());
#elif VEX_VULKAN
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile(
        reinterpret_cast<vk::VkFeatureChecker*>(featureChecker)->GetMaxSupportedSpirVVersion().data());

    // Force aliasing of bindless resources
    slang::CompilerOptionEntry entry{ .name = slang::CompilerOptionName::BindlessSpaceIndex,
                                      .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int,
                                                                           .intValue0 = 0 } };

    targetDesc.compilerOptionEntries = &entry;
    targetDesc.compilerOptionEntryCount = 1;
#endif

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    globalSession->createSession(sessionDesc, session.writeRef());
}

SlangCompilerImpl::~SlangCompilerImpl() = default;

std::expected<std::vector<u8>, std::string> SlangCompilerImpl::CompileShader(
    const Shader& shader,
    ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings,
    const ShaderResourceContext& resourceContext) const
{
    // TODO: finish implementing compilation with Slang. The current roadblock is getting ResourceDescriptorHeap to
    // work with both Vk and DX12.

    // For now we error out.
    VEX_NOT_YET_IMPLEMENTED();

    using namespace SlangCompilerImpl_Internal;

    // Create a compilation request
    Slang::ComPtr<slang::ICompileRequest> request;
    if (SLANG_FAILED(session->createCompileRequest(request.writeRef())) || !request)
    {
        return std::unexpected("Failed to create Slang compile request.");
    }

    // This matches DXC's layout.
    request->setMatrixLayoutMode(SLANG_MATRIX_LAYOUT_COLUMN_MAJOR);

    // Set compilation flags based on settings
    SlangCompileFlags flags = 0;
    if (compilerSettings.enableShaderDebugging)
    {
        request->setDebugInfoLevel(SLANG_DEBUG_INFO_LEVEL_MAXIMAL);
    }

    if (compilerSettings.enableHLSL202xFeatures)
    {
        // Ignored, Slang already includes HLSL 202x features.
    }

    request->setCompileFlags(flags);

    // Add shader environment defines
    for (const auto& define : shaderEnv.defines)
    {
        std::string name = WStringToString(define.name);
        std::string value = WStringToString(define.value);
        if (define.value.empty())
        {
            request->addPreprocessorDefine(name.c_str(), "1");
        }
        else
        {
            request->addPreprocessorDefine(name.c_str(), value.c_str());
        }
    }

    //// Add the shader source
    // i32 moduleIndex = request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_HLSL, nullptr);
    // request->addTranslationUnitSourceString(moduleIndex, shader.key.path.string().c_str(), shaderFileStr.c_str());

    // Set entry point based on shader type
    // if (!IsRayTracingShader(shader.key.type))
    //{
    //    SlangStage stage = GetSlangStageFromShaderType(shader.key.type);
    //    i32 entryPointIndex = request->addEntryPoint(moduleIndex, shader.key.entryPoint.c_str(), stage);
    //    if (entryPointIndex < 0)
    //    {
    //        return std::unexpected(std::format("Failed to add/find entry point '{}'.", shader.key.entryPoint));
    //    }
    //}
    // else
    //{
    //    // For ray tracing shaders, we might need different handling...
    //    SlangStage stage = GetSlangStageFromShaderType(shader.key.type);
    //    request->addEntryPoint(moduleIndex, shader.key.entryPoint.c_str(), stage);
    //}

    // Compile the shader
    SlangResult compilationResult = request->compile();

    // Extract the shader's dependencies.
    // for (u32 i = 0; i < request->getDependencyFileCount(); ++i)
    //{
    //    shaderDependencies.emplace_back(request->getDependencyFilePath(i));
    //}

    if (SLANG_FAILED(compilationResult))
    {
        // Get diagnostic messages
        const char* diagnostics = request->getDiagnosticOutput();
        std::string errorMsg = diagnostics ? std::string(diagnostics) : "Unknown compilation error";
        return std::unexpected(std::format("Slang shader compilation failed: {}.", errorMsg));
    }

    // Get the compiled bytecode
    Slang::ComPtr<slang::IBlob> bytecodeBlob;
    if (SLANG_FAILED(request->getEntryPointCodeBlob(0, 0, bytecodeBlob.writeRef())) || !bytecodeBlob)
    {
        return std::unexpected("Failed to get compiled shader bytecode from Slang.");
    }

    // Copy the bytecode to our return vector
    std::vector<u8> finalShaderBlob;
    std::size_t blobSize = bytecodeBlob->getBufferSize();
    finalShaderBlob.resize(blobSize);
    std::memcpy(finalShaderBlob.data(), bytecodeBlob->getBufferPointer(), blobSize);

    return finalShaderBlob;
}

void SlangCompilerImpl::FillInIncludeDirectories(std::vector<std::string> includeDirStrings,
                                                 std::vector<const char*> includeDirCStr,
                                                 slang::SessionDesc& desc)
{
    includeDirStrings.reserve(includeDirectories.size());
    includeDirCStr.reserve(includeDirectories.size());
    for (const auto& dir : includeDirectories)
    {
        includeDirStrings.emplace_back(dir.string());
        includeDirCStr.emplace_back(includeDirStrings.back().c_str());
    }
    desc.searchPathCount = includeDirCStr.size();
    desc.searchPaths = includeDirCStr.data();
}

} // namespace vex