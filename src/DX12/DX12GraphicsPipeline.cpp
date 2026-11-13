#include "DX12GraphicsPipeline.h"

#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>

#include <DX12/DX12Formats.h>

namespace vex::dx12
{

namespace GraphicsPipeline
{

D3D12_COMPARISON_FUNC GetDX12ComparisonFuncFromCompareOp(CompareOp compareOp)
{
    return static_cast<D3D12_COMPARISON_FUNC>(static_cast<int>(compareOp) + 1);
}

D3D12_STENCIL_OP GetDX12StencilOpFromStencilOp(StencilOp stencilOp)
{
    return static_cast<D3D12_STENCIL_OP>(static_cast<int>(stencilOp) + 1);
}

D3D12_BLEND GetDX12BlendFromBlendFactor(BlendFactor blendFactor)
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
        VEX_CHECK(GPhysicalDevice->SupportsAlphaBlendFactor(), "Your GPU does not support alpha blend factors.");
        return D3D12_BLEND_ALPHA_FACTOR;
    case BlendFactor::OneMinusConstantAlpha:
        VEX_CHECK(GPhysicalDevice->SupportsAlphaBlendFactor(), "Your GPU does not support alpha blend factors.");
        return D3D12_BLEND_INV_ALPHA_FACTOR;
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

D3D12_BLEND_OP GetDX12BlendOpFromBlendOp(BlendOp blendOp)
{
    return static_cast<D3D12_BLEND_OP>(static_cast<int>(blendOp) + 1);
}

D3D12_LOGIC_OP GetDX12LogicOpFromLogicOp(LogicOp logicOp)
{
    switch (logicOp)
    {
    case LogicOp::Clear:
        return D3D12_LOGIC_OP_CLEAR;
    case LogicOp::And:
        return D3D12_LOGIC_OP_AND;
    case LogicOp::AndReverse:
        return D3D12_LOGIC_OP_AND_REVERSE;
    case LogicOp::Copy:
        return D3D12_LOGIC_OP_COPY;
    case LogicOp::AndInverted:
        return D3D12_LOGIC_OP_AND_INVERTED;
    case LogicOp::NoOp:
        return D3D12_LOGIC_OP_NOOP;
    case LogicOp::Xor:
        return D3D12_LOGIC_OP_XOR;
    case LogicOp::Or:
        return D3D12_LOGIC_OP_OR;
    case LogicOp::Nor:
        return D3D12_LOGIC_OP_NOR;
    case LogicOp::Equivalent:
        return D3D12_LOGIC_OP_EQUIV;
    case LogicOp::Invert:
        return D3D12_LOGIC_OP_INVERT;
    case LogicOp::OrReverse:
        return D3D12_LOGIC_OP_OR_REVERSE;
    case LogicOp::CopyInverted:
        return D3D12_LOGIC_OP_COPY_INVERTED;
    case LogicOp::OrInverted:
        return D3D12_LOGIC_OP_OR_INVERTED;
    case LogicOp::Nand:
        return D3D12_LOGIC_OP_NAND;
    case LogicOp::Set:
        return D3D12_LOGIC_OP_SET;
    default:
        return D3D12_LOGIC_OP_NOOP;
    }
}

CD3DX12_RASTERIZER_DESC2 GetDX12RasterizerStateFromRasterizerState(const RasterizerState& rasterizerState)
{
    CD3DX12_RASTERIZER_DESC2 desc(D3D12_DEFAULT);

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
    desc.DepthBias = rasterizerState.depthBiasEnabled ? rasterizerState.depthBiasConstantFactor : 0;
    desc.DepthBiasClamp = rasterizerState.depthBiasEnabled ? rasterizerState.depthBiasClamp : 0;
    desc.SlopeScaledDepthBias = rasterizerState.depthBiasEnabled ? rasterizerState.depthBiasSlopeFactor : 0;
    desc.DepthClipEnable = !rasterizerState.depthClampEnabled; // Inverted logic

    return desc;
}

CD3DX12_BLEND_DESC GetDX12BlendStateFromColorBlendState(const ColorBlendState& blendState)
{
    CD3DX12_BLEND_DESC desc(D3D12_DEFAULT);

    // DX12 logic operations only use RenderTarget[0] and require blending to be disabled.
    desc.IndependentBlendEnable = !blendState.logicOpEnabled;
    desc.RenderTarget[0].LogicOpEnable = blendState.logicOpEnabled;
    desc.RenderTarget[0].LogicOp = GetDX12LogicOpFromLogicOp(blendState.logicOp);

    for (u32 i = 0;
         i < std::min<u32>(static_cast<u32>(blendState.attachments.size()), GMaxSimultaneousRenderTargetCount);
         ++i)
    {
        const auto& attachment = blendState.attachments[i];
        auto& renderTarget = desc.RenderTarget[i];

        VEX_CHECK(!blendState.logicOpEnabled || attachment.colorWriteMask == blendState.attachments[0].colorWriteMask,
                  "Logic operations require all render targets to use the same color write mask.");

        renderTarget.BlendEnable = attachment.blendEnabled && !blendState.logicOpEnabled;
        renderTarget.SrcBlend = GetDX12BlendFromBlendFactor(attachment.srcColorBlendFactor);
        renderTarget.DestBlend = GetDX12BlendFromBlendFactor(attachment.dstColorBlendFactor);
        renderTarget.BlendOp = GetDX12BlendOpFromBlendOp(attachment.colorBlendOp);
        renderTarget.SrcBlendAlpha = GetDX12BlendFromBlendFactor(attachment.srcAlphaBlendFactor);
        renderTarget.DestBlendAlpha = GetDX12BlendFromBlendFactor(attachment.dstAlphaBlendFactor);
        renderTarget.BlendOpAlpha = GetDX12BlendOpFromBlendOp(attachment.alphaBlendOp);

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

    // Note: Blend constants are dynamic state (see RHICommandList::SetBlendConstants).

    return desc;
}

CD3DX12_DEPTH_STENCIL_DESC2 GetDX12DepthStencilStateFromDepthStencilState(const DepthStencilState& depthStencilState)
{
    const DepthStencilState::StencilOpState& front = depthStencilState.front;
    const DepthStencilState::StencilOpState& back = depthStencilState.back;
    VEX_CHECK(GPhysicalDevice->SupportsIndependentFrontAndBackStencilRef() ||
                  (front.readMask == back.readMask && front.writeMask == back.writeMask),
              "Your GPU does not support independent front and back stencil masks, both masks must be equal.");

    static constexpr auto ToDX12StencilOpDesc = [](const DepthStencilState::StencilOpState& op)
    {
        return D3D12_DEPTH_STENCILOP_DESC1{
            .StencilFailOp = GetDX12StencilOpFromStencilOp(op.failOp),
            .StencilDepthFailOp = GetDX12StencilOpFromStencilOp(op.depthFailOp),
            .StencilPassOp = GetDX12StencilOpFromStencilOp(op.passOp),
            .StencilFunc = GetDX12ComparisonFuncFromCompareOp(op.compareOp),
            .StencilReadMask = op.readMask,
            .StencilWriteMask = op.writeMask,
        };
    };

    return CD3DX12_DEPTH_STENCIL_DESC2(D3D12_DEPTH_STENCIL_DESC2{
        .DepthEnable = depthStencilState.depthTestEnabled,
        .DepthWriteMask =
            depthStencilState.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = GetDX12ComparisonFuncFromCompareOp(depthStencilState.depthCompareOp),
        .StencilEnable = depthStencilState.stencilTestEnabled,
        .FrontFace = ToDX12StencilOpDesc(front),
        .BackFace = ToDX12StencilOpDesc(back),
        .DepthBoundsTestEnable = depthStencilState.depthBoundsTestEnabled,
    });
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
    case InputTopology::LineList:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case InputTopology::LineStrip:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
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
    case InputTopology::LineList:
    case InputTopology::LineStrip:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
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
        result[i] =
            TextureFormatToDXGI(renderTargetState.colorFormats[i].format, renderTargetState.colorFormats[i].isSRGB);
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