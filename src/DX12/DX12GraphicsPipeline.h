#pragma once

#include <array>
#include <vector>

#include <Vex/GraphicsPipeline.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

namespace GraphicsPipeline
{

D3D12_COMPARISON_FUNC GetD3D12ComparisonFuncFromCompareOp(CompareOp compareOp);
D3D12_STENCIL_OP GetD3D12StencilOpFromStencilOp(StencilOp stencilOp);
D3D12_BLEND GetD3D12BlendFromBlendFactor(BlendFactor blendFactor);
D3D12_BLEND_OP GetD3D12BlendOpFromBlendOp(BlendOp blendOp);
CD3DX12_RASTERIZER_DESC GetDX12RasterizerStateFromRasterizerState(const RasterizerState& rasterizerState);
CD3DX12_BLEND_DESC GetDX12BlendStateFromColorBlendState(const ColorBlendState& blendState);
D3D12_DEPTH_STENCIL_DESC GetDX12DepthStencilStateFromDepthStencilState(const DepthStencilState& depthStencilState);
std::vector<D3D12_INPUT_ELEMENT_DESC> GetDX12InputElementDescFromVertexInputAssembly(
    const VertexInputLayout& vertexInputLayout);
D3D12_PRIMITIVE_TOPOLOGY GetDX12PrimitiveTopologyFromInputAssembly(const InputAssembly& inputAssembly);
D3D12_PRIMITIVE_TOPOLOGY_TYPE GetDX12PrimitiveTopologyTypeFromInputAssembly(const InputAssembly& inputAssembly);
u32 GetNumRenderTargetsFromRenderTargetState(const RenderTargetState& renderTargetState);
std::array<DXGI_FORMAT, 8> GetRTVFormatsFromRenderTargetState(const RenderTargetState& renderTargetState);

} // namespace GraphicsPipeline

} // namespace vex::dx12