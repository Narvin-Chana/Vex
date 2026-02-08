#include "SlangImpl.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>

namespace vex
{

namespace SlangImpl_Internal
{

std::expected<Slang::ComPtr<slang::IBlob>, std::string> GetByteCode(slang::IComponentType* linkedProgram)
{
    // Get the compiled bytecode
    i32 entryPointIndex = 0; // only one entry point
    i32 targetIndex = 0;     // only one target
    Slang::ComPtr<ISlangBlob> bytecodeBlob;
    Slang::ComPtr<ISlangBlob> diagnostics = nullptr;
    if (SLANG_FAILED(linkedProgram->getEntryPointCode(entryPointIndex,
                                                      targetIndex,
                                                      bytecodeBlob.writeRef(),
                                                      diagnostics.writeRef())) ||
        !bytecodeBlob || diagnostics)
    {
        return std::unexpected(std::format("Failed to get compiled shader bytecode: {}.",
                                           static_cast<const char*>(diagnostics->getBufferPointer())));
    }
    return bytecodeBlob;
}

SHA1HashDigest GetProgramHash(slang::IComponentType* linkedProgram)
{
    slang::IBlob* blob;
    linkedProgram->getEntryPointHash(0, 0, &blob);
    SHA1HashDigest hash{};
    std::uninitialized_copy_n(static_cast<const u32*>(blob->getBufferPointer()), 5, hash.data());
    return hash;
}

std::expected<NonNullPtr<slang::IModule>, std::string> LoadModule(Slang::ComPtr<slang::ISession> session,
                                                                  const ShaderKey& shaderKey)
{
    Slang::ComPtr<ISlangBlob> diagnostics;

    slang::IModule* slangModule = nullptr;
    if (!shaderKey.path.empty())
    {
        // loadModule compiles the shader with the passed-in name (searches in the IFileSystem).
        slangModule = session->loadModule(shaderKey.path.string().c_str(), diagnostics.writeRef());
    }
    else
    {
        // Used for identifying the shader inside the session.
        // Should be unique per compilation session, which is why we give it a slightly convoluted name.
        constexpr const char* moduleName = "VEX_InlineShaderModule";
        slangModule = session->loadModuleFromSourceString(moduleName,
                                                          nullptr,
                                                          shaderKey.sourceCode.c_str(),
                                                          diagnostics.writeRef());
    }

    if (!slangModule || diagnostics)
    {
        return std::unexpected(
            std::format("Unable to loadModule: {}", static_cast<const char*>(diagnostics->getBufferPointer())));
    }
    return NonNullPtr{ slangModule };
}

std::expected<Slang::ComPtr<slang::IEntryPoint>, std::string> FindEntryPoint(slang::IModule* module,
                                                                             const std::string& entryPointName)
{
    // Obtain the entry point corresponding to our shader.
    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    if (SLANG_FAILED(module->findEntryPointByName(entryPointName.c_str(), entryPoint.writeRef())) || !entryPoint)
    {
        return std::unexpected(std::format("Unable to fetch/find entry point: {}", entryPointName));
    }
    return entryPoint;
}

std::expected<Slang::ComPtr<slang::IComponentType>, std::string> LinkProgram(
    const Slang::ComPtr<slang::IComponentType>& program)
{
    Slang::ComPtr<ISlangBlob> diagnostics = nullptr;
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef())) || diagnostics)
    {
        return std::unexpected(
            std::format("Link error: {}", static_cast<const char*>(diagnostics->getBufferPointer())));
    }
    return linkedProgram;
}

std::expected<Slang::ComPtr<slang::IComponentType>, std::string> GetShaderProgram(
    const Slang::ComPtr<slang::ISession>& session,
    const NonNullPtr<slang::IModule> module,
    const Slang::ComPtr<slang::IEntryPoint>& entryPoint)
{
    std::array<slang::IComponentType*, 2> components = { module, entryPoint };
    Slang::ComPtr<slang::IComponentType> program;
    if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), program.writeRef())) ||
        !program)
    {
        return std::unexpected("Unable to create composite component type.");
    }
    return program;
}

std::expected<Slang::ComPtr<slang::IComponentType>, std::string> GetLinkedShader(
    const Slang::ComPtr<slang::ISession>& session, const ShaderKey& shaderKey)
{
    return LoadModule(session, shaderKey)
        .and_then(
            [&](const NonNullPtr<slang::IModule>& module)
            {
                return FindEntryPoint(module, shaderKey.entryPoint)
                    .and_then([&](const Slang::ComPtr<slang::IEntryPoint>& entryPoint)
                              { return GetShaderProgram(session, module, entryPoint); });
            })
        .and_then(&LinkProgram);
}

static TextureFormat SlangTypeToFormat(slang::TypeReflection* type)
{
    auto kind = type->getKind();

    if (kind == slang::TypeReflection::Kind::Vector)
    {
        unsigned count = type->getElementCount();
        switch (type->getScalarType())
        {
        case slang::TypeReflection::ScalarType::Float32:
            switch (count)
            {
            case 2:
                return TextureFormat::RG32_FLOAT;
            case 3:
                return TextureFormat::RGB32_FLOAT;
            case 4:
                return TextureFormat::RGBA32_FLOAT;
            default:;
            }
        case slang::TypeReflection::ScalarType::Float16:
            switch (count)
            {
            case 2:
                return TextureFormat::RG16_FLOAT;
            case 4:
                return TextureFormat::RGBA16_FLOAT;
            default:;
            }
        case slang::TypeReflection::ScalarType::Int32:
            switch (count)
            {
            case 2:
                return TextureFormat::RG32_SINT;
            case 3:
                return TextureFormat::RGB32_SINT;
            case 4:
                return TextureFormat::RGBA32_SINT;
            default:;
            }
        case slang::TypeReflection::ScalarType::Int16:
            switch (count)
            {
            case 2:
                return TextureFormat::RG16_SINT;
            case 4:
                return TextureFormat::RGBA16_SINT;
            default:;
            }
        case slang::TypeReflection::ScalarType::Int8:
            switch (count)
            {
            case 2:
                return TextureFormat::RG8_SINT;
            case 4:
                return TextureFormat::RGBA8_SINT;
            default:;
            }
        case slang::TypeReflection::ScalarType::UInt32:
            switch (count)
            {
            case 2:
                return TextureFormat::RG32_UINT;
            case 3:
                return TextureFormat::RGB32_UINT;
            case 4:
                return TextureFormat::RGBA32_UINT;
            default:;
            }
        case slang::TypeReflection::ScalarType::UInt16:
            switch (count)
            {
            case 2:
                return TextureFormat::RG16_UINT;
            case 4:
                return TextureFormat::RGBA16_UINT;
            default:;
            }
        case slang::TypeReflection::ScalarType::UInt8:
            switch (count)
            {
            case 2:
                return TextureFormat::RG8_UINT;
            case 4:
                return TextureFormat::RGBA8_UINT;
            default:;
            }
        default:;
        }
    }
    else if (kind == slang::TypeReflection::Kind::Scalar)
    {
        switch (type->getScalarType())
        {
        case slang::TypeReflection::ScalarType::Float32:
            return TextureFormat::R32_FLOAT;
        case slang::TypeReflection::ScalarType::Float16:
            return TextureFormat::R16_FLOAT;

        case slang::TypeReflection::ScalarType::Int32:
            return TextureFormat::R32_SINT;
        case slang::TypeReflection::ScalarType::Int16:
            return TextureFormat::R16_SINT;
        case slang::TypeReflection::ScalarType::Int8:
            return TextureFormat::R8_SINT;

        case slang::TypeReflection::ScalarType::UInt32:
            return TextureFormat::R32_UINT;
        case slang::TypeReflection::ScalarType::UInt16:
            return TextureFormat::R16_UINT;
        case slang::TypeReflection::ScalarType::UInt8:
            return TextureFormat::R8_UINT;
        default:;
        }
    }

    return TextureFormat::UNKNOWN;
}

static ShaderReflection GetSlangReflection(slang::IComponentType* program)
{
    slang::ProgramLayout* reflection = program->getLayout();
    slang::EntryPointReflection* entryPoint = reflection->getEntryPointByIndex(0);

    ShaderReflection reflectionData;

    auto TryAddShaderInput = [&reflectionData](const ShaderReflection::Input& input)
    {
        if (!ShaderUtil::IsBuiltInSemantic(input.semanticName))
        {
            reflectionData.inputs.push_back(input);
        }
    };

    for (u32 i = 0; i < entryPoint->getParameterCount(); ++i)
    {
        slang::VariableLayoutReflection* param = entryPoint->getParameterByIndex(i);
        if (param->getCategory() == slang::VaryingInput)
        {
            // If semantic name is null we need to look into the struct for the vertex input semantics
            if (param->getSemanticName() == nullptr)
            {
                slang::TypeLayoutReflection* paramLayout = param->getTypeLayout();
                for (u32 j = 0; j < paramLayout->getFieldCount(); j++)
                {
                    slang::VariableLayoutReflection* field = paramLayout->getFieldByIndex(j);
                    TryAddShaderInput({ field->getSemanticName(),
                                        static_cast<u32>(field->getSemanticIndex()),
                                        SlangTypeToFormat(field->getType()) });
                }
            }
            else
            {
                TryAddShaderInput({ param->getSemanticName(),
                                    static_cast<u32>(param->getSemanticIndex()),
                                    SlangTypeToFormat(param->getType()) });
            }
        }
    }

    return reflectionData;
}

} // namespace SlangImpl_Internal

SlangCompilerImpl::SlangCompilerImpl(std::vector<std::filesystem::path> incDirs)
    : CompilerBase(std::move(incDirs))
{
    slang::createGlobalSession(globalSession.writeRef());
}

SlangCompilerImpl::~SlangCompilerImpl() = default;

std::expected<SHA1HashDigest, std::string> SlangCompilerImpl::GetShaderCodeHash(
    const Shader& shader, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings)
{
    ValidateShaderForCompilation(shader);

    return CreateSession(shader.key, shaderEnv, compilerSettings)
        .and_then([&](const Slang::ComPtr<slang::ISession>& session)
                  { return SlangImpl_Internal::GetLinkedShader(session, shader.key); })
        .and_then(
            [&](const Slang::ComPtr<slang::IComponentType>& linkedShader) -> std::expected<SHA1HashDigest, std::string>
            { return SlangImpl_Internal::GetProgramHash(linkedShader); });
}

std::expected<ShaderCompilationResult, std::string> SlangCompilerImpl::CompileShader(
    const Shader& shader, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const
{
    ValidateShaderForCompilation(shader);

    return CreateSession(shader.key, shaderEnv, compilerSettings)
        .and_then([&](const Slang::ComPtr<slang::ISession>& session)
                  { return SlangImpl_Internal::GetLinkedShader(session, shader.key); })
        .and_then(
            [&](const Slang::ComPtr<slang::IComponentType>& linkedProgram)
            {
                return SlangImpl_Internal::GetByteCode(linkedProgram)
                    .and_then(
                        [&](const Slang::ComPtr<slang::IBlob>& bytecodeBlob)
                            -> std::expected<ShaderCompilationResult, std::string>
                        {
                            // Copy the bytecode to our return vector
                            std::vector<byte> finalShaderBlob;
                            std::size_t blobSize = bytecodeBlob->getBufferSize();
                            finalShaderBlob.resize(blobSize);
                            std::memcpy(finalShaderBlob.data(), bytecodeBlob->getBufferPointer(), blobSize);

                            std::optional<ShaderReflection> reflection;
                            if (ShaderUtil::CanReflectShaderType(shader.key.type))
                            {
                                reflection = SlangImpl_Internal::GetSlangReflection(linkedProgram);
                            }
                            return ShaderCompilationResult{ SlangImpl_Internal::GetProgramHash(linkedProgram),
                                                            finalShaderBlob,
                                                            reflection };
                        });
            });
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

std::expected<Slang::ComPtr<slang::ISession>, std::string> SlangCompilerImpl::CreateSession(
    const ShaderKey& key, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const
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

    // Add shader environment and shader key defines.
    std::vector<slang::PreprocessorMacroDesc> slangDefines;
    slangDefines.reserve(shaderEnv.defines.size() + key.defines.size());
    std::ranges::transform(shaderEnv.defines,
                           std::back_inserter(slangDefines),
                           [](const ShaderDefine& d) -> slang::PreprocessorMacroDesc
                           { return { d.name.c_str(), d.value.c_str() }; });
    std::ranges::transform(key.defines,
                           std::back_inserter(slangDefines),
                           [](const ShaderDefine& d) -> slang::PreprocessorMacroDesc
                           { return { d.name.c_str(), d.value.c_str() }; });
    sessionDesc.preprocessorMacros = slangDefines.data();
    sessionDesc.preprocessorMacroCount = slangDefines.size();

    // Configure target for both DX12 and Vulkan
#if VEX_DX12
    std::string highestSupportedShaderModel = std::string(magic_enum::enum_name(GPhysicalDevice->GetShaderModel()));
    // s and m must be lower-case (eg: SM_6_6 -> sm_6_6).
    highestSupportedShaderModel[0] = std::tolower(highestSupportedShaderModel[0]);
    highestSupportedShaderModel[1] = std::tolower(highestSupportedShaderModel[1]);
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = globalSession->findProfile(highestSupportedShaderModel.c_str());
#elif VEX_VULKAN
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile(GPhysicalDevice->GetMaxSupportedSpirVVersion().data());

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

    // Forces SPIR-V code to use DX's layout for buffers
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::ForceDXLayout,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int, .intValue0 = true } });

    // Emit reflection information.
    compilerOptions.push_back(
        { .name = slang::CompilerOptionName::VulkanEmitReflection,
          .value = slang::CompilerOptionValue{ .kind = slang::CompilerOptionValueKind::Int, .intValue0 = true } });
#endif

    targetDesc.compilerOptionEntries = compilerOptions.data();
    targetDesc.compilerOptionEntryCount = compilerOptions.size();

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())))
    {
        return std::unexpected("Failed to create Slang session");
    }
    return session;
}

void SlangCompilerImpl::ValidateShaderForCompilation(const Shader& shader) const
{
    const bool useFilepath = !shader.key.path.empty();
    if (useFilepath && !shader.key.sourceCode.empty())
    {
        VEX_LOG(Warning,
                "Shader {} has both a shader filepath and shader source string. Using the filepath for compilation...",
                shader.key);
    }

    if (useFilepath && shader.key.path.extension() != ".slang")
    {
        VEX_LOG(Fatal,
                "Slang shaders must use a .slang file format, your extension: {}!",
                shader.key.path.extension().string());
    }
}

} // namespace vex