#include "DXCImplReflection.h"

#include <bitset>

#include <Vex/Platform/Debug.h>

#if VEX_DX12
#include <d3d12shader.h>

#include <Vex/Shaders/DXCImpl.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12Headers.h>
#endif

#if VEX_VULKAN
#include <spirv_reflect.h>
#endif

namespace vex
{

#if VEX_VULKAN
TextureFormat SpirvReflectFormatToVex(SpvReflectFormat format)
{
    switch (format)
    {
    case SPV_REFLECT_FORMAT_R16_UINT:
        return TextureFormat::R16_UINT;
    case SPV_REFLECT_FORMAT_R16_SINT:
        return TextureFormat::R16_SINT;
    case SPV_REFLECT_FORMAT_R16_SFLOAT:
        return TextureFormat::R16_FLOAT;
    case SPV_REFLECT_FORMAT_R16G16_UINT:
        return TextureFormat::RG16_UINT;
    case SPV_REFLECT_FORMAT_R16G16_SINT:
        return TextureFormat::RG16_SINT;
    case SPV_REFLECT_FORMAT_R16G16_SFLOAT:
        return TextureFormat::RG16_FLOAT;
    case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:
        return TextureFormat::RGBA16_UINT;
    case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:
        return TextureFormat::RGBA16_SINT;
    case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:
        return TextureFormat::RGBA16_FLOAT;
    case SPV_REFLECT_FORMAT_R32_UINT:
        return TextureFormat::R32_UINT;
    case SPV_REFLECT_FORMAT_R32_SINT:
        return TextureFormat::R32_SINT;
    case SPV_REFLECT_FORMAT_R32_SFLOAT:
        return TextureFormat::R32_FLOAT;
    case SPV_REFLECT_FORMAT_R32G32_UINT:
        return TextureFormat::RG32_UINT;
    case SPV_REFLECT_FORMAT_R32G32_SINT:
        return TextureFormat::RG32_SINT;
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
        return TextureFormat::RG32_FLOAT;
    case SPV_REFLECT_FORMAT_R32G32B32_UINT:
        return TextureFormat::RGB32_UINT;
    case SPV_REFLECT_FORMAT_R32G32B32_SINT:
        return TextureFormat::RGB32_SINT;
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
        return TextureFormat::RGB32_FLOAT;
    case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
        return TextureFormat::RGBA32_UINT;
    case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
        return TextureFormat::RGBA32_SINT;
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
        return TextureFormat::RGBA32_FLOAT;
    case SPV_REFLECT_FORMAT_R64_UINT:
    case SPV_REFLECT_FORMAT_R64_SINT:
    case SPV_REFLECT_FORMAT_R64_SFLOAT:
    case SPV_REFLECT_FORMAT_R64G64_UINT:
    case SPV_REFLECT_FORMAT_R64G64_SINT:
    case SPV_REFLECT_FORMAT_R64G64_SFLOAT:
    case SPV_REFLECT_FORMAT_R64G64B64_UINT:
    case SPV_REFLECT_FORMAT_R64G64B64_SINT:
    case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:
    case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:
    case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:
    case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:
    case SPV_REFLECT_FORMAT_R16G16B16_UINT:
    case SPV_REFLECT_FORMAT_R16G16B16_SINT:
    case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:
    default:
        return TextureFormat::UNKNOWN;
    }
}

ShaderReflection GetSpirvReflection(Span<const byte> spvCode)
{
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spvCode.size_bytes(), spvCode.data(), &module);
    VEX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    // Enumerate and extract shader's input variables
    uint32_t var_count = 0;
    result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    VEX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable*> inputVariables{ var_count };
    result = spvReflectEnumerateInputVariables(&module, &var_count, inputVariables.data());
    VEX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    ShaderReflection reflectionData;
    for (u32 i = 0; i < inputVariables.size(); ++i)
    {
        SpvReflectInterfaceVariable* input = inputVariables[i];

        std::string semanticName = input->semantic ? input->semantic : "";
        // Trim left most numeric chars
        auto j = semanticName.size() - 1;
        while (j > 0 && isdigit(semanticName[j]))
            --j;

        int semanticIndex = 0;
        if (j < semanticName.size() - 1)
        {
            semanticIndex = std::stoi(semanticName.substr(j + 1));
        }
        semanticName.resize(j + 1);
        reflectionData.inputs.emplace_back(semanticName, semanticIndex, SpirvReflectFormatToVex(input->format));
    }

    // Output variables, descriptor bindings, descriptor sets, and push constants
    // can be enumerated and extracted using a similar mechanism.

    // Destroy the reflection data when no longer required.
    spvReflectDestroyShaderModule(&module);

    return reflectionData;
}
#endif

#if VEX_DX12
TextureFormat CreateFormatFromMaskAndType(std::bitset<8> mask, D3D_REGISTER_COMPONENT_TYPE component)
{
    if (mask == 0b1111) // RGBA
    {
        switch (component)
        {
        case D3D_REGISTER_COMPONENT_UINT32:
            return TextureFormat::RGBA32_UINT;
        case D3D_REGISTER_COMPONENT_SINT32:
            return TextureFormat::RGBA32_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT32:
            return TextureFormat::RGBA32_FLOAT;
        case D3D_REGISTER_COMPONENT_UINT16:
            return TextureFormat::RGBA16_UINT;
        case D3D_REGISTER_COMPONENT_SINT16:
            return TextureFormat::RGBA16_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT16:
            return TextureFormat::RGBA16_FLOAT;
        default:
            return TextureFormat::UNKNOWN;
        }
    }

    if (mask == 0b111) // RGB
    {
        switch (component)
        {
        case D3D_REGISTER_COMPONENT_UINT32:
            return TextureFormat::RGB32_UINT;
        case D3D_REGISTER_COMPONENT_SINT32:
            return TextureFormat::RGB32_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT32:
            return TextureFormat::RGB32_FLOAT;
        default:
            return TextureFormat::UNKNOWN;
        }
    }

    if (mask == 0b11) // RG
    {
        switch (component)
        {
        case D3D_REGISTER_COMPONENT_UINT32:
            return TextureFormat::RG32_UINT;
        case D3D_REGISTER_COMPONENT_SINT32:
            return TextureFormat::RG32_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT32:
            return TextureFormat::RG32_FLOAT;
        case D3D_REGISTER_COMPONENT_UINT16:
            return TextureFormat::RG16_UINT;
        case D3D_REGISTER_COMPONENT_SINT16:
            return TextureFormat::RG16_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT16:
            return TextureFormat::RG16_FLOAT;
        default:
            return TextureFormat::UNKNOWN;
        }
    }

    if (mask == 0b1) // R
    {
        switch (component)
        {
        case D3D_REGISTER_COMPONENT_UINT32:
            return TextureFormat::R32_UINT;
        case D3D_REGISTER_COMPONENT_SINT32:
            return TextureFormat::R32_SINT;
        case D3D_REGISTER_COMPONENT_FLOAT32:
            return TextureFormat::R32_FLOAT;
        default:
            return TextureFormat::UNKNOWN;
        }
    }

    return TextureFormat::UNKNOWN;
}

ShaderReflection GetDxcReflection(dx12::ComPtr<IDxcResult> compilationResult)
{
    ComPtr<IDxcBlob> reflectionBlob{};
    HRESULT res = compilationResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);
    if (FAILED(res))
    {
        VEX_ASSERT(false);
    }

    const DxcBuffer reflectionBuffer{
        .Ptr = reflectionBlob->GetBufferPointer(),
        .Size = reflectionBlob->GetBufferSize(),
        .Encoding = 0,
    };

    ComPtr<IDxcUtils> utils;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    ComPtr<ID3D12ShaderReflection> shaderReflection{};
    utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));

    D3D12_SHADER_DESC shaderDesc{};
    shaderReflection->GetDesc(&shaderDesc);

    ShaderReflection reflectionData;
    for (u32 i = 0; i < shaderDesc.InputParameters; ++i)
    {
        D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
        shaderReflection->GetInputParameterDesc(i, &signatureParameterDesc);

        std::bitset<8> typeBits(signatureParameterDesc.Mask);
        reflectionData.inputs.push_back({
            .semanticName = signatureParameterDesc.SemanticName,
            .semanticIndex = signatureParameterDesc.SemanticIndex,
            .format = CreateFormatFromMaskAndType(typeBits, signatureParameterDesc.ComponentType),
        });
    }

    return reflectionData;
}
#endif

} // namespace vex
