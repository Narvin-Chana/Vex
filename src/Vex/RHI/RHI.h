#pragma once

#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct PhysicalDevice;
class RHICommandPool;
class RHICommandList;
class RHIFence;
class RHISwapChain;
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

    virtual void ExecuteCommandList(RHICommandList& commandList) = 0;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) = 0;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex