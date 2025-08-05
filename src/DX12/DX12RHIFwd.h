#pragma once

namespace vex::dx12
{

class DX12CommandList;
class DX12CommandPool;
class DX12DescriptorPool;
class DX12GraphicsPipelineState;
class DX12ComputePipelineState;
class DX12SwapChain;
class DX12ResourceLayout;
class DX12Texture;
class DX12Buffer;
class DX12RHI;

} // namespace vex::dx12

namespace vex
{

using RHICommandList = dx12::DX12CommandList;
using RHICommandPool = dx12::DX12CommandPool;
using RHIDescriptorPool = dx12::DX12DescriptorPool;
using RHIGraphicsPipelineState = dx12::DX12GraphicsPipelineState;
using RHIComputePipelineState = dx12::DX12ComputePipelineState;
using RHISwapChain = dx12::DX12SwapChain;
using RHIResourceLayout = dx12::DX12ResourceLayout;
using RHITexture = dx12::DX12Texture;
using RHIBuffer = dx12::DX12Buffer;
using RHI = dx12::DX12RHI;

} // namespace vex