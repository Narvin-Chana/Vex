#include "DXCImpl.h"

#include <string_view>

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Shaders/ShaderResourceContext.h>

namespace vex
{

namespace DXCImpl_Internal
{

static constexpr std::array HLSL202xFlags{
    L"-HV 202x", L"-Wconversion", L"-Wdouble-promotion", L"-Whlsl-legacy-literal"
};

static std::wstring GetTargetFromShaderType(ShaderType type)
{
    FeatureChecker* featureChecker = GPhysicalDevice->featureChecker.get();

    std::wstring highestSupportedShaderModel =
        StringToWString(std::string(magic_enum::enum_name(featureChecker->GetShaderModel())));

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
    case ShaderType::RayGenerationShader:
    case ShaderType::RayMissShader:
    case ShaderType::RayClosestHitShader:
    case ShaderType::RayAnyHitShader:
    case ShaderType::RayIntersectionShader:
    case ShaderType::RayCallableShader:
        return L"lib" + highestSupportedShaderModel.substr(2);
    default:
        VEX_LOG(Fatal, "Unsupported shader type for the Vex ShaderCompiler.");
        break;
    }

    // Second character is always 's' for non-RT shaders.
    highestSupportedShaderModel[1] = L's';

    return highestSupportedShaderModel;
}

static std::vector<DxcDefine> ConvertDefinesToDxcDefine(const std::vector<ShaderDefine>& defines)
{
    std::vector<DxcDefine> dxcDefines;
    dxcDefines.reserve(defines.size());
    std::ranges::transform(defines,
                           std::back_inserter(dxcDefines),
                           [](const ShaderDefine& d)
                           { return DxcDefine{ .Name = d.name.c_str(), .Value = d.value.c_str() }; });
    return dxcDefines;
}

} // namespace DXCImpl_Internal

DXCCompilerImpl::DXCCompilerImpl(std::vector<std::filesystem::path> includeDirectories)
    : CompilerBase(std::move(includeDirectories))
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

DXCCompilerImpl::~DXCCompilerImpl() = default;

std::expected<std::vector<u8>, std::string> DXCCompilerImpl::CompileShader(
    const Shader& shader,
    ShaderEnvironment& shaderEnv,
    const ShaderCompilerSettings& compilerSettings,
    const ShaderResourceContext& resourceContext) const
{
    using namespace DXCImpl_Internal;

    ComPtr<IDxcBlobEncoding> shaderBlob;
    if (HRESULT hr = utils->LoadFile(shader.key.path.c_str(), nullptr, &shaderBlob); FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to load shader from filesystem at path: {}.", WStringToString(shader.key.path)));
    }

    DxcBuffer shaderSource = {};
    shaderSource.Ptr = shaderBlob->GetBufferPointer();
    shaderSource.Size = shaderBlob->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    std::vector<LPCWSTR> args;
    args.reserve(shaderEnv.args.size());
    for (const std::wstring& arg : shaderEnv.args)
    {
        args.emplace_back(arg.c_str());
    }
    std::ranges::transform(shaderEnv.args,
                           std::back_inserter(args),
                           [](const std::wstring& arg) { return arg.c_str(); });

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
        args.insert(args.end(), HLSL202xFlags.begin(), HLSL202xFlags.end());
    }

    FillInIncludeDirectories(args);

    // Add in the shader bindings for the user requested resources.
    GenerateVexBindings(shaderEnv.defines, resourceContext);

    std::vector<DxcDefine> dxcDefines = ConvertDefinesToDxcDefine(shaderEnv.defines);

    std::wstring entryPoint;
    // RT Shaders are compiled differently to other shader types, the entry point should be null.
    if (!IsRayTracingShader(shader.key.type))
    {
        entryPoint = StringToWString(shader.key.entryPoint);
    }

    ComPtr<IDxcCompilerArgs> compilerArgs;
    if (HRESULT hr = utils->BuildArguments(shader.key.path.wstring().c_str(),
                                           entryPoint.empty() ? nullptr : entryPoint.c_str(),
                                           GetTargetFromShaderType(shader.key.type).c_str(),
                                           args.data(),
                                           static_cast<u32>(args.size()),
                                           dxcDefines.data(),
                                           static_cast<u32>(dxcDefines.size()),
                                           &compilerArgs);
        FAILED(hr))
    {
        return std::unexpected(
            std::format("Failed to build shader compilation arguments from file: {}", shader.key.path.string()));
    }

    ComPtr<IDxcResult> shaderCompilationResults;
    HRESULT compilationHR = compiler->Compile(&shaderSource,
                                              compilerArgs->GetArguments(),
                                              compilerArgs->GetCount(),
                                              defaultIncludeHandler.operator->(),
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
    std::vector<u8> finalShaderBlob;
    finalShaderBlob.resize(shaderBytecode->GetBufferSize());
    std::memcpy(finalShaderBlob.data(), shaderBytecode->GetBufferPointer(), finalShaderBlob.size() * sizeof(u8));

    return finalShaderBlob;
}

void DXCCompilerImpl::FillInIncludeDirectories(std::vector<LPCWSTR>& args) const
{
    for (auto& additionalIncludeFolder : includeDirectories)
    {
        args.insert(args.end(), { L"-I", StringToWString(additionalIncludeFolder.string()).c_str() });
    }
}

void DXCCompilerImpl::GenerateVexBindings(std::vector<ShaderDefine>& defines,
                                          const ShaderResourceContext& resourceContext) const
{
    std::wstring shaderBindings;

    // TODO: Should ensure no duplicate binding names... this should be done in CommandContext
    // Will be done in a second PR.

    shaderBindings.append(L"struct Vex_GeneratedGlobalResources {");
    for (const auto& [binding, _] : resourceContext.textures)
    {
        if (binding.usage == TextureBindingUsage::None)
        {
            continue;
        }

        shaderBindings.append(std::format(L"    uint {}_BindlessIndex; ", StringToWString(binding.name)));
    }
    for (const auto& [binding, _] : resourceContext.buffers)
    {
        if (binding.usage == BufferBindingUsage::Invalid)
        {
            continue;
        }

        shaderBindings.append(std::format(L"    uint {}_BindlessIndex; ", StringToWString(binding.name)));
    }

    static constexpr std::wstring_view LocalConstantsShaderName = L"LocalConstants";

#if VEX_VULKAN
    shaderBindings.append(L"};"
                          "struct Vex_GeneratedCombinedResources"
                          "{"
                          "    uint GlobalResourcesBindlessIndex;");

    if (resourceContext.constantBinding.has_value())
    {
        std::wstring typeName = StringToWString(resourceContext.constantBinding->typeName);
        shaderBindings.append(std::format(L"\t{} UserData;", typeName));

        // Connects the LocalConstantsShaderName to the actual data stored inside the push constants.
        defines.emplace_back(std::wstring(LocalConstantsShaderName),
                             std::format(L"{} (Vex_GeneratedCombinedResourcesCB.UserData)", typeName));
    }
    shaderBindings.append(
        L"}; "
        "[[vk::push_constant]] ConstantBuffer<Vex_GeneratedCombinedResources> Vex_GeneratedCombinedResourcesCB; "
        "static ConstantBuffer<Vex_GeneratedGlobalResources> Vex_GeneratedGlobalResourcesCB = "
        "ResourceDescriptorHeap[Vex_GeneratedCombinedResourcesCB.GlobalResourcesBindlessIndex]; ");

#elif VEX_DX12
    shaderBindings.append(std::format(
        L"}}; ConstantBuffer<Vex_GeneratedGlobalResources> Vex_GeneratedGlobalResourcesCB : register(b0); "));

    if (resourceContext.constantBinding.has_value())
    {
        shaderBindings.append(std::format(L"ConstantBuffer<{}> {} : register(b1); ",
                                          StringToWString(resourceContext.constantBinding->typeName),
                                          LocalConstantsShaderName));
    }
#endif

    auto GetTextureHLSLDeclarationFromBinding = [](TextureBinding binding) -> std::wstring
    {
        std::wstring typeName;
        switch (binding.usage)
        {
        case TextureBindingUsage::ShaderRead:
        {
            typeName = L"Texture";
            break;
        }
        case TextureBindingUsage::ShaderReadWrite:
        {
            typeName = L"RWTexture";
            break;
        }
        default:
            VEX_LOG(Fatal, "Unsupported texture binding usage for DXC shader compiler...");
        }

        std::wstring textureDimension;
        switch (binding.texture.description.type)
        {
        case TextureType::Texture2D:
        {
            if (binding.texture.description.depthOrArraySize <= 1)
            {
                textureDimension = L"2D";
            }
            else
            {
                textureDimension = L"2DArray";
            }
            break;
        }
        case TextureType::TextureCube:
        {
            if (binding.texture.description.depthOrArraySize <= 1)
            {
                textureDimension = L"Cube";
            }
            else
            {
                textureDimension = L"CubeArray";
            }
            break;
        }
        case TextureType::Texture3D:
        {
            textureDimension = L"3D";
            // Texture3DArray doesn't exist in HLSL.
            break;
        }
        }

        return std::format(
            L"static {0}{1}<{2}> {3} = ResourceDescriptorHeap[Vex_GeneratedGlobalResourcesCB.{3}_BindlessIndex];",
            typeName,
            textureDimension,
            StringToWString(binding.typeName),
            StringToWString(binding.name));
    };
    for (const auto& [binding, _] : resourceContext.textures)
    {
        if (binding.usage == TextureBindingUsage::None)
        {
            continue;
        }

        shaderBindings.append(GetTextureHLSLDeclarationFromBinding(binding));
    }

    auto GetBufferHLSLDeclarationFromBinding = [](BufferBinding binding) -> std::wstring
    {
        std::wstring typeName;
        switch (binding.usage)
        {
        case BufferBindingUsage::ConstantBuffer:
            typeName = L"ConstantBuffer";
            break;
        case BufferBindingUsage::StructuredBuffer:
            typeName = L"StructuredBuffer";
            break;
        case BufferBindingUsage::RWStructuredBuffer:
            typeName = L"RWStructuredBuffer";
            break;
        case BufferBindingUsage::ByteAddressBuffer:
            typeName = L"ByteAddressBuffer";
            break;
        case BufferBindingUsage::RWByteAddressBuffer:
            typeName = L"RWByteAddressBuffer";
            break;
        default:
            VEX_LOG(Fatal, "Unsupported buffer binding usage for DXC shader compiler...");
        }
        if (binding.typeName.has_value())
        {
            return std::format(
                L"static {0}<{1}> {2} = ResourceDescriptorHeap[Vex_GeneratedGlobalResourcesCB.{2}_BindlessIndex];",
                typeName,
                StringToWString(binding.typeName.value()),
                StringToWString(binding.name));
        }
        else
        {
            return std::format(
                L"static {0} {1} = ResourceDescriptorHeap[Vex_GeneratedGlobalResourcesCB.{1}_BindlessIndex];",
                typeName,
                StringToWString(binding.name));
        }
    };
    for (const auto& [binding, _] : resourceContext.buffers)
    {
        if (binding.usage == BufferBindingUsage::Invalid)
        {
            continue;
        }

        shaderBindings.append(GetBufferHLSLDeclarationFromBinding(binding));
    }

    // Auto-generate shader static sampler bindings.
    for (u32 i = 0; i < resourceContext.samplers.size(); ++i)
    {
        const TextureSampler& sampler = resourceContext.samplers[i];
        shaderBindings.append(
            std::format(L"SamplerState {} : register(s{}, space0); ", StringToWString(sampler.name), i));
    }

    defines.emplace_back(L"VEX_SHADER_BINDINGS", shaderBindings);

    // Defines for obtaining bindless resources directly from an index.
    defines.emplace_back(L"VEX_GET_BINDLESS_RESOURCE(index)", L"ResourceDescriptorHeap[index]");
}

} // namespace vex