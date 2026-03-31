#include "SlangCompiler.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Utility/NonNullPtr.h>

#include <ShaderCompiler/Shader.h>
#include <ShaderCompiler/ShaderCompiler.h>
#include <ShaderCompiler/ShaderCompilerSettings.h>
#include <ShaderCompiler/ShaderKey.h>

namespace vex::sc
{

namespace SlangImpl_Internal
{

std::expected<Slang::ComPtr<slang::IBlob>, std::string> GetByteCode(slang::IComponentType* linkedProgram)
{
    // Get the compiled bytecode
    static constexpr i32 EntryPointIndex = 0; // only one entry point
    static constexpr i32 TargetIndex = 0;     // only one target
    Slang::ComPtr<ISlangBlob> bytecodeBlob;
    Slang::ComPtr<ISlangBlob> diagnostics = nullptr;
    if (SLANG_FAILED(linkedProgram->getEntryPointCode(EntryPointIndex,
                                                      TargetIndex,
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

std::expected<NonNullPtr<slang::IModule>, std::string> LoadModule(const Slang::ComPtr<slang::ISession>& session,
                                                                  const std::filesystem::path& filepath)
{
    Slang::ComPtr<ISlangBlob> diagnostics;

    // loadModule compiles the shader with the passed-in name (searches in the IFileSystem).
    slang::IModule* slangModule = session->loadModule(filepath.string().c_str(), diagnostics.writeRef());

    if (!slangModule || diagnostics)
    {
        return std::unexpected(
            std::format("Unable to loadModule: {}", static_cast<const char*>(diagnostics->getBufferPointer())));
    }
    return NonNullPtr{ slangModule };
}

std::expected<NonNullPtr<slang::IModule>, std::string> LoadModule(const Slang::ComPtr<slang::ISession>& session,
                                                                  const ShaderKey& shaderKey,
                                                                  const std::string_view sourceCode)
{
    Slang::ComPtr<ISlangBlob> diagnostics;
    slang::IModule* slangModule = session->loadModuleFromSourceString(shaderKey.filepath.c_str(),
                                                                      nullptr,
                                                                      std::string(sourceCode).c_str(),
                                                                      diagnostics.writeRef());
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
    const std::array<slang::IComponentType*, 2> components = { module, entryPoint };
    Slang::ComPtr<slang::IComponentType> program;
    if (SLANG_FAILED(session->createCompositeComponentType(components.data(), components.size(), program.writeRef())) ||
        !program)
    {
        return std::unexpected("Unable to create composite component type.");
    }
    return program;
}

std::expected<Slang::ComPtr<slang::IComponentType>, std::string> GetLinkedShaderProgram(
    const ShaderKey& key, const Slang::ComPtr<slang::ISession>& session, const NonNullPtr<slang::IModule> module)
{
    std::expected<Slang::ComPtr<slang::IEntryPoint>, std::string> entryPointRes =
        FindEntryPoint(module, key.entryPoint);
    if (!entryPointRes)
    {
        return std::unexpected(entryPointRes.error());
    }

    std::expected<Slang::ComPtr<slang::IComponentType>, std::string> shaderProgramRes =
        GetShaderProgram(session, module, entryPointRes.value());
    if (!shaderProgramRes.has_value())
    {
        return std::unexpected(shaderProgramRes.error());
    }

    return LinkProgram(shaderProgramRes.value());
}

static TextureFormat SlangTypeToFormat(slang::TypeReflection* type)
{
    const slang::TypeReflection::Kind kind = type->getKind();

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

SlangCompiler::SlangCompiler(std::vector<std::filesystem::path> incDirs)
    : CompilerBase(std::move(incDirs))
{
    slang::createGlobalSession(globalSession.writeRef());
}

std::expected<ShaderCompilationResult, std::string> SlangCompiler::CompileShader(
    const Shader& shader,
    const std::filesystem::path& filepath,
    const ShaderEnvironment& environment,
    const ShaderCompilerSettings& compilerSettings)
{
    auto sessionRes = CreateSession(shader.GetKey(), environment, compilerSettings);
    if (!sessionRes)
    {
        return std::unexpected(sessionRes.error());
    }

    auto moduleRes = SlangImpl_Internal::LoadModule(sessionRes.value(), filepath);
    if (!moduleRes)
    {
        return std::unexpected(moduleRes.error());
    }

    return CompileFromModule(shader, sessionRes.value(), moduleRes.value());
}

std::expected<ShaderCompilationResult, std::string> SlangCompiler::CompileShader(
    const Shader& shader,
    const std::string_view sourceCode,
    const ShaderEnvironment& environment,
    const ShaderCompilerSettings& compilerSettings)
{
    auto sessionRes = CreateSession(shader.GetKey(), environment, compilerSettings);
    if (!sessionRes)
    {
        return std::unexpected(sessionRes.error());
    }

    auto moduleRes = SlangImpl_Internal::LoadModule(sessionRes.value(), shader.GetKey(), sourceCode);
    if (!moduleRes)
    {
        return std::unexpected(moduleRes.error());
    }

    return CompileFromModule(shader, sessionRes.value(), moduleRes.value());
}

SHA1HashDigest SlangCompiler::GetShaderCodeHash(const Slang::ComPtr<slang::IComponentType>& linkedShaderProgram)
{
    slang::IBlob* blob;
    linkedShaderProgram->getEntryPointHash(0, 0, &blob);
    SHA1HashDigest hash{};
    std::uninitialized_copy_n(static_cast<const u32*>(blob->getBufferPointer()), 5, hash.data());
    return hash;
}

std::expected<ShaderCompilationResult, std::string> SlangCompiler::CompileFromModule(
    const Shader& shader, const Slang::ComPtr<slang::ISession>& session, const NonNullPtr<slang::IModule> module)
{
    auto linkedShaderProgramRes = SlangImpl_Internal::GetLinkedShaderProgram(shader.GetKey(), session, module);
    if (!linkedShaderProgramRes)
    {
        return std::unexpected(linkedShaderProgramRes.error());
    }

    const SHA1HashDigest programHash = SlangImpl_Internal::GetProgramHash(linkedShaderProgramRes.value());
    if (shader.IsValid() && programHash == shader.GetHash())
    {
        // Nothing to recompile, return the already existing shader.
        return ShaderCompilationResult{ shader.GetHash(),
                                        { shader.GetBlob().begin(), shader.GetBlob().end() },
                                        *shader.GetReflection() };
    }

    auto bytecodeBlobRes = SlangImpl_Internal::GetByteCode(linkedShaderProgramRes.value());
    if (!bytecodeBlobRes)
    {
        return std::unexpected(bytecodeBlobRes.error());
    }

    // Copy the bytecode to our return vector
    std::vector<byte> finalShaderBlob;
    const std::size_t blobSize = bytecodeBlobRes.value()->getBufferSize();
    finalShaderBlob.resize(blobSize);
    std::memcpy(finalShaderBlob.data(), bytecodeBlobRes.value()->getBufferPointer(), blobSize);

    std::optional<ShaderReflection> reflection;
    if (ShaderUtil::CanReflectShaderType(shader.GetKey().type))
    {
        reflection = SlangImpl_Internal::GetSlangReflection(linkedShaderProgramRes.value());
    }
    return ShaderCompilationResult{ programHash, finalShaderBlob, reflection };
}

void SlangCompiler::FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
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

std::expected<Slang::ComPtr<slang::ISession>, std::string> SlangCompiler::CreateSession(
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
    if (compilerSettings.enableShaderDebugSymbols)
    {
        compilerOptions.emplace_back(slang::CompilerOptionName::DebugInformation,
                                     slang::CompilerOptionValue{ .intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL });
    }
    compilerOptions.emplace_back(
        slang::CompilerOptionName::Optimization,
        slang::CompilerOptionValue{ .intValue0 = static_cast<i32>(compilerSettings.enableShaderOptimizations
                                                                      ? SLANG_OPTIMIZATION_LEVEL_MAXIMAL
                                                                      : SLANG_OPTIMIZATION_LEVEL_NONE) });

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
    if (compilerSettings.target == CompilationTarget::DXIL)
    {
        std::string highestSupportedShaderModel = std::string(magic_enum::enum_name(compilerSettings.shaderModel));
        // s and m must be lower-case (eg: SM_6_6 -> sm_6_6).
        highestSupportedShaderModel[0] = std::tolower(highestSupportedShaderModel[0]);
        highestSupportedShaderModel[1] = std::tolower(highestSupportedShaderModel[1]);
        targetDesc.format = SLANG_DXIL;
        targetDesc.profile = globalSession->findProfile(highestSupportedShaderModel.c_str());
    }
    else if (compilerSettings.target == CompilationTarget::SPIRV)
    {
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = globalSession->findProfile(magic_enum::enum_name(compilerSettings.spirvVersion).data());

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
    }

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

} // namespace vex::sc