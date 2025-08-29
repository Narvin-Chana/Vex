#include "SlangImpl.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>

#if VEX_VULKAN
#include <Vulkan/VkFeatureChecker.h>
#endif

namespace vex
{

SlangCompilerImpl::SlangCompilerImpl(std::vector<std::filesystem::path> incDirs)
    : CompilerBase(std::move(incDirs))
{
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();

    slang::createGlobalSession(globalSession.writeRef());
}

SlangCompilerImpl::~SlangCompilerImpl() = default;

std::expected<std::vector<u8>, std::string> SlangCompilerImpl::CompileShader(
    const Shader& shader, ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const
{
    if (shader.key.path.extension() != ".slang")
    {
        VEX_LOG(Fatal,
                "Slang shaders must use a .slang file format, your extension: {}!",
                shader.key.path.extension().string());
    }

    Slang::ComPtr<slang::ISession> session = CreateSession(shader, shaderEnv, compilerSettings);

    Slang::ComPtr<ISlangBlob> diagnostics;

    // loadModule compiles the shader with the passed-in name (searches in the IFileSystem).
    slang::IModule* module = session->loadModule(shader.key.path.string().c_str(), diagnostics.writeRef());
    if (!module || diagnostics)
    {
        return std::unexpected(
            std::format("Unable to loadModule: {}", static_cast<const char*>(diagnostics->getBufferPointer())));
    }

    // Obtain the entry point corresponding to our shader.
    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    if (SLANG_FAILED(module->findEntryPointByName(shader.key.entryPoint.c_str(), entryPoint.writeRef())) || !entryPoint)
    {
        return std::unexpected(std::format("Unable to fetch/find entry point: {}", shader.key.entryPoint));
    }

    std::array<slang::IComponentType*, 2> components = { module, entryPoint };
    Slang::ComPtr<slang::IComponentType> program;
    if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), program.writeRef())) ||
        !program)
    {
        return std::unexpected("Unable to create composite component type.");
    }

    // TODO: this is where reflection would be done (using program)

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    diagnostics = nullptr;
    if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef())) || diagnostics)
    {
        return std::unexpected(
            std::format("Link error: {}", static_cast<const char*>(diagnostics->getBufferPointer())));
    }

    // Get the compiled bytecode
    i32 entryPointIndex = 0; // only one entry point
    i32 targetIndex = 0;     // only one target
    Slang::ComPtr<ISlangBlob> bytecodeBlob;
    diagnostics = nullptr;
    if (SLANG_FAILED(linkedProgram->getEntryPointCode(entryPointIndex,
                                                      targetIndex,
                                                      bytecodeBlob.writeRef(),
                                                      diagnostics.writeRef())) ||
        !bytecodeBlob || diagnostics)
    {
        return std::unexpected(std::format("Failed to get compiled shader bytecode: {}.",
                                           static_cast<const char*>(diagnostics->getBufferPointer())));
    }

    // Copy the bytecode to our return vector
    std::vector<u8> finalShaderBlob;
    std::size_t blobSize = bytecodeBlob->getBufferSize();
    finalShaderBlob.resize(blobSize);
    std::memcpy(finalShaderBlob.data(), bytecodeBlob->getBufferPointer(), blobSize);

    return finalShaderBlob;
}

void SlangCompilerImpl::FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
                                                 std::vector<const char*>& includeDirCStr,
                                                 slang::SessionDesc& desc) const
{
    includeDirStrings.reserve(includeDirectories.size() + 1);
    includeDirCStr.reserve(includeDirectories.size() + 1);
    for (const auto& dir : includeDirectories)
    {
        includeDirStrings.emplace_back(dir.string());
        includeDirCStr.emplace_back(includeDirStrings.back().c_str());
    }

    // Also adds the current working directory for the Vex.slang file.
    includeDirStrings.emplace_back(std::filesystem::current_path().string());
    includeDirCStr.emplace_back(includeDirStrings.back().c_str());

    desc.searchPathCount = includeDirCStr.size();
    desc.searchPaths = includeDirCStr.data();
}

Slang::ComPtr<slang::ISession> SlangCompilerImpl::CreateSession(const Shader& shader,
                                                                ShaderEnvironment& shaderEnv,
                                                                const ShaderCompilerSettings& compilerSettings) const
{
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

    // Add include directories
    std::vector<std::string> stringPaths;
    // std::filesystem::c_str() returns wchar on windows, slang takes in chars, so we must have two vectors for the
    // conversion (one to store the strings, one for the actual c_strs).
    std::vector<const char*> cstrPaths;
    FillInIncludeDirectories(stringPaths, cstrPaths, sessionDesc);

    // This matches DXC's layout.
    sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    // Set compilation flags based on settings
    std::vector<slang::CompilerOptionEntry> compilerOptions;
    if (compilerSettings.enableShaderDebugging)
    {
        compilerOptions.emplace_back(slang::CompilerOptionName::DebugInformation,
                                     slang::CompilerOptionValue{ .intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL });
    }

    if (compilerSettings.enableHLSL202xFeatures)
    {
        // Ignored, Slang already natively includes most HLSL 202x features.
    }

    // Add shader environment defines.
    std::vector<slang::PreprocessorMacroDesc> slangDefines;
    slangDefines.reserve(shaderEnv.defines.size());
    std::ranges::transform(shaderEnv.defines,
                           std::back_inserter(slangDefines),
                           [](const ShaderDefine& d) -> slang::PreprocessorMacroDesc
                           { return { d.name.c_str(), d.value.c_str() }; });
    sessionDesc.preprocessorMacros = slangDefines.data();
    sessionDesc.preprocessorMacroCount = slangDefines.size();

    // Configure target for both DX12 and Vulkan
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();
#if VEX_DX12
    std::string highestSupportedShaderModel = std::string(magic_enum::enum_name(featureChecker->GetShaderModel()));
    // s and m must be lower-case (eg: SM_6_6 -> sm_6_6).
    highestSupportedShaderModel[0] = std::tolower(highestSupportedShaderModel[0]);
    highestSupportedShaderModel[1] = std::tolower(highestSupportedShaderModel[1]);
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = globalSession->findProfile(highestSupportedShaderModel.c_str());
#elif VEX_VULKAN
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile(
        reinterpret_cast<vk::VkFeatureChecker*>(featureChecker)->GetMaxSupportedSpirVVersion().data());

    // Required for DescriptorHandle<T> to work.
    SlangCapabilityID rtCapability = globalSession->findCapability("spvRayTracingKHR");
    compilerOptions.push_back({ .name = slang::CompilerOptionName::Capability,
                                .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int,
                                                                     .intValue0 = rtCapability } });

    // Force direct SPIR-V compilation (avoids passing through a downstream compiler, in this case glslang-compiler).
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::EmitSpirvMethod,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int,
                                               .intValue0 = SlangEmitSpirvMethod::SLANG_EMIT_SPIRV_DIRECTLY } });

    // Allow for entry point names other than 'main' (GLSL only allows one entry point per-file, spirv does not have
    // this limitation).
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::VulkanUseEntryPointName,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int, .intValue0 = true } });

    // Emit reflection information.
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::VulkanEmitReflection,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int, .intValue0 = true } });

    // Force aliasing of bindless resources to bindless set 0.
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::BindlessSpaceIndex,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 0 } });
#endif

    targetDesc.compilerOptionEntries = compilerOptions.data();
    targetDesc.compilerOptionEntryCount = compilerOptions.size();

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    Slang::ComPtr<slang::ISession> session;
    globalSession->createSession(sessionDesc, session.writeRef());
    return session;
}

} // namespace vex