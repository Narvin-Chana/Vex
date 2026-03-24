#include "SlangImpl.h"

#include <algorithm>
#include <atomic>
#include <fstream>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Platform.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompileContext.h>
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
        return std::unexpected(std::format("Can't find the module in either fs"));
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

struct StringBlob : public ISlangBlob
{
    std::string data;
    std::atomic<uint32_t> refCount = 1;

    StringBlob(std::string d)
        : data(std::move(d))
    {
    }

    virtual SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) override
    {
        if (uuid == SLANG_UUID_ISlangBlob || uuid == SLANG_UUID_ISlangUnknown)
        {
            *outObject = static_cast<ISlangBlob*>(this);
            addRef();
            return SLANG_OK;
        }
        *outObject = nullptr;
        return SLANG_E_NO_INTERFACE;
    }

    virtual SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
    {
        return ++refCount;
    }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL release() override
    {
        uint32_t count = --refCount;
        if (count == 0)
            delete this;
        return count;
    }
    // ISlangBlob
    virtual SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() override
    {
        return data.data();
    }
    virtual SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override
    {
        return data.size();
    }
};

class CustomSlangFileSystem : public ISlangFileSystem
{
public:
    ShaderCompileContext* context;
    std::atomic<uint32_t> refCount = 1;

    CustomSlangFileSystem(ShaderCompileContext* context)
        : context(context)
    {
    }

    virtual SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) override
    {
        if (uuid == SLANG_UUID_ISlangFileSystem || uuid == SLANG_UUID_ISlangUnknown)
        {
            *outObject = static_cast<ISlangFileSystem*>(this);
            addRef();
            return SLANG_OK;
        }
        *outObject = nullptr;
        return SLANG_E_NO_INTERFACE;
    }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override
    {
        return ++refCount;
    }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL release() override
    {
        uint32_t count = --refCount;
        if (count == 0)
            delete this;
        return count;
    }
    virtual SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID& guid) override
    {
        if (guid == SLANG_UUID_ISlangFileSystem)
            return static_cast<ISlangFileSystem*>(this);
        return nullptr;
    }

    virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const* path, ISlangBlob** outBlob) override
    {
        if (context)
        {
            auto& vfs = context->GetVirtualFiles();
            auto it = vfs.find(path);
            if (it != vfs.end())
            {
                *outBlob = new StringBlob(it->second);
                return SLANG_OK;
            }
        }
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return SLANG_E_NOT_FOUND;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string buffer(size, '\0');
        if (file.read(buffer.data(), size))
        {
            *outBlob = new StringBlob(std::move(buffer));
            return SLANG_OK;
        }
        return SLANG_E_CANNOT_OPEN;
    }
};

SlangCompilerContextImpl::SlangCompilerContextImpl(const ShaderEnvironment& env,
                                                   const ShaderCompilerSettings& compilerSettings,
                                                   ShaderCompileContext* parentContext)
    : parentContext(parentContext)
{
}

SlangCompilerContextImpl::~SlangCompilerContextImpl() = default;

bool SlangCompilerContextImpl::LoadModule(const std::string& moduleName)
{
    if (session)
    {
        slang::IModule* module = session->loadModule(moduleName.c_str());
        return module != nullptr;
    }
    return false;
}

SlangCompilerImpl::SlangCompilerImpl(std::vector<std::filesystem::path> incDirs)
    : CompilerBase(std::move(incDirs))
{
    slang::createGlobalSession(globalSession.writeRef());
}

SlangCompilerImpl::~SlangCompilerImpl() = default;

std::unique_ptr<ICompilerContextImpl> SlangCompilerImpl::CreateContext(const ShaderEnvironment& env,
                                                                       const ShaderCompilerSettings& compilerSettings,
                                                                       ShaderCompileContext* context) const
{
    auto contextImpl = std::make_unique<SlangCompilerContextImpl>(env, compilerSettings, context);
    // Explicitly create an ISession for the context which will cache modules loaded into it
    ShaderKey dummyKey;
    if (auto sessionExp = CreateSession(dummyKey, env, compilerSettings, context))
    {
        contextImpl->session = sessionExp.value();
    }
    return contextImpl;
}

std::expected<SHA1HashDigest, std::string> SlangCompilerImpl::GetShaderCodeHash(
    const Shader& shader,
    const ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings,
    ShaderCompileContext* context)
{
    ValidateShaderForCompilation(shader);

    Slang::ComPtr<slang::ISession> session;
    if (context && context->GetImpl<SlangCompilerContextImpl>() &&
        context->GetImpl<SlangCompilerContextImpl>()->session)
    {
        session = context->GetImpl<SlangCompilerContextImpl>()->session;
    }
    else
    {
        auto sessionRes = CreateSession(shader.key, shaderEnv, compilerSettings, context);
        if (!sessionRes)
            return std::unexpected(sessionRes.error());
        session = sessionRes.value();
    }

    return SlangImpl_Internal::GetLinkedShader(session, shader.key)
        .and_then(
            [&](const Slang::ComPtr<slang::IComponentType>& linkedShader) -> std::expected<SHA1HashDigest, std::string>
            { return SlangImpl_Internal::GetProgramHash(linkedShader); });
}

std::expected<ShaderCompilationResult, std::string> SlangCompilerImpl::CompileShader(
    const Shader& shader,
    const ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings,
    ShaderCompileContext* context) const
{
    ValidateShaderForCompilation(shader);

    Slang::ComPtr<slang::ISession> session;
    if (context && context->GetImpl<SlangCompilerContextImpl>() &&
        context->GetImpl<SlangCompilerContextImpl>()->session)
    {
        session = context->GetImpl<SlangCompilerContextImpl>()->session;
    }
    else
    {
        auto sessionRes = CreateSession(shader.key, shaderEnv, compilerSettings, context);
        if (!sessionRes)
            return std::unexpected(sessionRes.error());
        session = sessionRes.value();
    }

    auto linkedProgramRes = SlangImpl_Internal::GetLinkedShader(session, shader.key);
    if (!linkedProgramRes)
        return std::unexpected(linkedProgramRes.error());
    const Slang::ComPtr<slang::IComponentType>& linkedProgram = linkedProgramRes.value();

    return SlangImpl_Internal::GetByteCode(linkedProgram)
        .and_then(
            [&](const Slang::ComPtr<slang::IBlob>& bytecodeBlob) -> std::expected<ShaderCompilationResult, std::string>
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
    const ShaderKey& key,
    const ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings,
    ShaderCompileContext* context) const
{
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};

    Slang::ComPtr<CustomSlangFileSystem> customFileSystem;
    if (context)
    {
        customFileSystem = new CustomSlangFileSystem(context);
        sessionDesc.fileSystem = customFileSystem;
    }

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
    if ( shader.key.path.extension() != ".slang")
    {
        VEX_LOG(Fatal,
                "Slang shaders must use a .slang file format, your extension: {}!",
                shader.key.path.extension().string());
    }
}

} // namespace vex