#pragma once

#include <span>
#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/Containers/FreeList.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

#include <Vex/RHI/RHIFwd.h>

namespace vex
{

struct ShaderDefine;
struct PhysicalDevice;
struct ShaderKey;
struct GraphicsPipelineStateKey;
struct ComputePipelineStateKey;
struct SwapChainDescription;
struct PlatformWindow;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) = 0;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() = 0;

    virtual UniqueHandle<RHIShader> CreateShader(const ShaderKey& key) = 0;
    virtual UniqueHandle<RHIGraphicsPipelineState> CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) = 0;
    virtual UniqueHandle<RHIComputePipelineState> CreateComputePipelineState(const ComputePipelineStateKey& key) = 0;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) = 0;

    virtual UniqueHandle<RHITexture> CreateTexture(const TextureDescription& description) = 0;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() = 0;

    virtual void ExecuteCommandList(RHICommandList& commandList) = 0;
    virtual void ExecuteCommandLists(std::span<RHICommandList*> commandLists) = 0;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) = 0;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;

    virtual void ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args,
                                                 std::vector<ShaderDefine>& defines) = 0;

    virtual UniqueHandle<RHIAccessor> CreateRHIAccessor(RHIDescriptorPool& descriptorPool) = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex