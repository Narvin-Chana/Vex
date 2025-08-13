#include "DX12GraphicsPipeline.h"

#include <DX12/DX12Formats.h>

namespace vex::dx12
{

namespace GraphicsPipeline
{

D3D12_COMPARISON_FUNC GetD3D12ComparisonFuncFromCompareOp(CompareOp compareOp)
{
    if (compareOp == CompareOp::None)
    {
        return D3D12_COMPARISON_FUNC_NONE;
    }
    return static_cast<D3D12_COMPARISON_FUNC>(static_cast<int>(compareOp) + 1);
}

D3D12_STENCIL_OP GetD3D12StencilOpFromStencilOp(StencilOp stencilOp)
{
    return static_cast<D3D12_STENCIL_OP>(static_cast<int>(stencilOp) + 1);
}

D3D12_BLEND GetD3D12BlendFromBlendFactor(BlendFactor blendFactor)
{
    switch (blendFactor)
    {
    case BlendFactor::Zero:
        return D3D12_BLEND_ZERO;
    case BlendFactor::One:
        return D3D12_BLEND_ONE;
    case BlendFactor::SrcColor:
        return D3D12_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFactor::DstColor:
        return D3D12_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDstColor:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFactor::SrcAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DstAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    case BlendFactor::ConstantColor:
        return D3D12_BLEND_BLEND_FACTOR;
    case BlendFactor::OneMinusConstantColor:
        return D3D12_BLEND_INV_BLEND_FACTOR;
    case BlendFactor::ConstantAlpha:
        return D3D12_BLEND_BLEND_FACTOR; // DX12 doesn't separate color/alpha constants
    case BlendFactor::OneMinusConstantAlpha:
        return D3D12_BLEND_INV_BLEND_FACTOR;
    case BlendFactor::SrcAlphaSaturate:
        return D3D12_BLEND_SRC_ALPHA_SAT;
    case BlendFactor::Src1Color:
        return D3D12_BLEND_SRC1_COLOR;
    case BlendFactor::OneMinusSrc1Color:
        return D3D12_BLEND_INV_SRC1_COLOR;
    case BlendFactor::Src1Alpha:
        return D3D12_BLEND_SRC1_ALPHA;
    case BlendFactor::OneMinusSrc1Alpha:
        return D3D12_BLEND_INV_SRC1_ALPHA;
    default:
        return D3D12_BLEND_ZERO;
    }
}

D3D12_BLEND_OP GetD3D12BlendOpFromBlendOp(BlendOp blendOp)
{
    return static_cast<D3D12_BLEND_OP>(static_cast<int>(blendOp) + 1);
}

CD3DX12_RASTERIZER_DESC GetDX12RasterizerStateFromRasterizerState(const RasterizerState& rasterizerState)
{
    CD3DX12_RASTERIZER_DESC desc(D3D12_DEFAULT);

    // Convert fill mode
    if (rasterizerState.polygonMode == PolygonMode::Line)
    {
        desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    }
    else
    {
        desc.FillMode = D3D12_FILL_MODE_SOLID;
    }

    // Convert cull mode
    switch (rasterizerState.cullMode)
    {
    case CullMode::None:
        desc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    case CullMode::Front:
        desc.CullMode = D3D12_CULL_MODE_FRONT;
        break;
    case CullMode::Back:
        desc.CullMode = D3D12_CULL_MODE_BACK;
        break;
    }

    // Convert winding order
    desc.FrontCounterClockwise = (rasterizerState.winding == Winding::CounterClockwise);

    // Depth bias
    desc.DepthBias = static_cast<INT>(rasterizerState.depthBiasConstantFactor);
    desc.DepthBiasClamp = rasterizerState.depthBiasClamp;
    desc.SlopeScaledDepthBias = rasterizerState.depthBiasSlopeFactor;
    desc.DepthClipEnable = !rasterizerState.depthClampEnabled; // Inverted logic

    // Note: DX12 doesn't have direct equivalents for rasterizerDiscardEnabled and lineWidth, we ignore them.

    return desc;
}

CD3DX12_BLEND_DESC GetDX12BlendStateFromColorBlendState(const ColorBlendState& blendState)
{
    CD3DX12_BLEND_DESC desc(D3D12_DEFAULT);

    // DX12 doesn't support logic operations in the same way as Vulkan, we ignore them.

    for (u32 i = 0; i < std::min<u32>(static_cast<u32>(blendState.attachments.size()), 8); ++i)
    {
        const auto& attachment = blendState.attachments[i];
        auto& renderTarget = desc.RenderTarget[i];

        renderTarget.BlendEnable = attachment.blendEnabled;
        renderTarget.SrcBlend = GetD3D12BlendFromBlendFactor(attachment.srcColorBlendFactor);
        renderTarget.DestBlend = GetD3D12BlendFromBlendFactor(attachment.dstColorBlendFactor);
        renderTarget.BlendOp = GetD3D12BlendOpFromBlendOp(attachment.colorBlendOp);
        renderTarget.SrcBlendAlpha = GetD3D12BlendFromBlendFactor(attachment.srcAlphaBlendFactor);
        renderTarget.DestBlendAlpha = GetD3D12BlendFromBlendFactor(attachment.dstAlphaBlendFactor);
        renderTarget.BlendOpAlpha = GetD3D12BlendOpFromBlendOp(attachment.alphaBlendOp);

        // Convert color write mask
        renderTarget.RenderTargetWriteMask = 0;
        if (attachment.colorWriteMask & ColorWriteMask::Red)
            renderTarget.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED;
        if (attachment.colorWriteMask & ColorWriteMask::Green)
            renderTarget.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
        if (attachment.colorWriteMask & ColorWriteMask::Blue)
            renderTarget.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (attachment.colorWriteMask & ColorWriteMask::Alpha)
            renderTarget.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    }

    // Note: DX12 doesn't have blend constants in the same way, we set them when binding the PSO.

    return desc;
}

D3D12_DEPTH_STENCIL_DESC GetDX12DepthStencilStateFromDepthStencilState(const DepthStencilState& depthStencilState)
{
    D3D12_DEPTH_STENCIL_DESC desc = {};

    desc.DepthEnable = depthStencilState.depthTestEnabled;
    desc.DepthWriteMask =
        depthStencilState.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = GetD3D12ComparisonFuncFromCompareOp(depthStencilState.depthCompareOp);

    // Stencil test
    desc.StencilEnable = depthStencilState.stencilTestEnabled;
    desc.StencilReadMask = static_cast<UINT8>(depthStencilState.front.readMask);
    desc.StencilWriteMask = static_cast<UINT8>(depthStencilState.front.writeMask);

    // Front face stencil
    desc.FrontFace.StencilFailOp = GetD3D12StencilOpFromStencilOp(depthStencilState.front.failOp);
    desc.FrontFace.StencilDepthFailOp = GetD3D12StencilOpFromStencilOp(depthStencilState.front.depthFailOp);
    desc.FrontFace.StencilPassOp = GetD3D12StencilOpFromStencilOp(depthStencilState.front.passOp);
    desc.FrontFace.StencilFunc = GetD3D12ComparisonFuncFromCompareOp(depthStencilState.front.compareOp);

    // Back face stencil
    desc.BackFace.StencilFailOp = GetD3D12StencilOpFromStencilOp(depthStencilState.back.failOp);
    desc.BackFace.StencilDepthFailOp = GetD3D12StencilOpFromStencilOp(depthStencilState.back.depthFailOp);
    desc.BackFace.StencilPassOp = GetD3D12StencilOpFromStencilOp(depthStencilState.back.passOp);
    desc.BackFace.StencilFunc = GetD3D12ComparisonFuncFromCompareOp(depthStencilState.back.compareOp);

    // Note: DX12 doesn't support depth bounds testing or per-face stencil masks/references, we ignore them.

    return desc;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> GetDX12InputElementDescFromVertexInputAssembly(
    const VertexInputLayout& vertexInputLayout)
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
    inputElements.reserve(vertexInputLayout.attributes.size());

    for (const auto& attr : vertexInputLayout.attributes)
    {
        D3D12_INPUT_ELEMENT_DESC elementDesc = {};
        elementDesc.SemanticName = attr.semanticName.c_str();
        elementDesc.SemanticIndex = attr.semanticIndex;
        elementDesc.Format = TextureFormatToDXGI(attr.format);
        elementDesc.InputSlot = attr.binding;
        elementDesc.AlignedByteOffset = attr.offset;

        // Find the corresponding binding to determine input slot class
        auto bindingIt = std::find_if(vertexInputLayout.bindings.begin(),
                                      vertexInputLayout.bindings.end(),
                                      [&](const VertexInputLayout::VertexBinding& binding)
                                      { return binding.binding == attr.binding; });

        if (bindingIt != vertexInputLayout.bindings.end())
        {
            elementDesc.InputSlotClass = (bindingIt->inputRate == VertexInputLayout::InputRate::PerInstance)
                                             ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                             : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            elementDesc.InstanceDataStepRate =
                (bindingIt->inputRate == VertexInputLayout::InputRate::PerInstance) ? 1 : 0;
        }
        else
        {
            elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            elementDesc.InstanceDataStepRate = 0;
        }

        inputElements.push_back(elementDesc);
    }

    return inputElements;
}

D3D12_PRIMITIVE_TOPOLOGY GetDX12PrimitiveTopologyFromInputAssembly(const InputAssembly& inputAssembly)
{
    switch (inputAssembly.topology)
    {
    case InputTopology::TriangleList:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case InputTopology::TriangleStrip:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case InputTopology::TriangleFan:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN;
    default:
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE GetDX12PrimitiveTopologyTypeFromInputAssembly(const InputAssembly& inputAssembly)
{
    switch (inputAssembly.topology)
    {
    case InputTopology::TriangleList:
    case InputTopology::TriangleStrip:
    case InputTopology::TriangleFan:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

u32 GetNumRenderTargetsFromRenderTargetState(const RenderTargetState& renderTargetState)
{
    return static_cast<u32>(renderTargetState.colorFormats.size());
}

std::array<DXGI_FORMAT, 8> GetRTVFormatsFromRenderTargetState(const RenderTargetState& renderTargetState)
{
    std::array<DXGI_FORMAT, 8> result;

    for (u32 i = 0; i < std::min<u32>(static_cast<u32>(renderTargetState.colorFormats.size()), 8); ++i)
    {
        result[i] = TextureFormatToDXGI(renderTargetState.colorFormats[i]);
    }

    // Fill remaining slots with UNKNOWN
    for (u32 i = static_cast<u32>(renderTargetState.colorFormats.size()); i < 8; ++i)
    {
        result[i] = DXGI_FORMAT_UNKNOWN;
    }

    return result;
}

} // namespace GraphicsPipeline

} // namespace vex::dx12