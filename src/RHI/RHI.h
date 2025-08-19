#pragma once

#include <span>
#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/FrameResource.h>
#include <Vex/RHIFwd.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct BufferDescription;
struct TextureDescription;
struct ShaderDefine;
struct PhysicalDevice;
struct GraphicsPipelineStateKey;
struct ComputePipelineStateKey;
struct SwapChainDescription;
struct PlatformWindow;

struct RHIBase
{
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) = 0;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() = 0;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) = 0;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) = 0;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) = 0;

    virtual RHITexture CreateTexture(const TextureDescription& description) = 0;
    virtual RHIBuffer CreateBuffer(const BufferDescription& description) = 0;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() = 0;

    virtual void ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args,
                                                 std::vector<ShaderDefine>& defines) = 0;

    virtual u32 AcquireNextFrame(RHISwapChain& swapChain, u32 currentFrameIndex) = 0;
    virtual void SubmitAndPresent(std::span<RHICommandList*> commandLists,
                                  RHISwapChain& swapChain,
                                  u32 currentFrameIndex,
                                  bool isFullscreenMode) = 0;
    virtual void FlushGPU() = 0;
};

} // namespace vex