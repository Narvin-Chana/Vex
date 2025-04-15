#pragma once

#include <vector>

#include <Vex/EnumFlags.h>
#include <Vex/PhysicalDevice.h>

namespace vex
{

class RHICommandPool;
class RHICommandList;
class RHIFence;

namespace CommandQueueTypes
{

enum Value : u8
{
    // Transfer-only operations
    Copy = 0,
    // Compute operations (includes Copy capabilites)
    Compute = 1,
    // Graphics operations (includes Compute and Copy capabilities)
    Graphics = 2,
};

// Not using a Count enum value allows us to more easily iterate over the CommandQueueTypes with magic_enum.
static constexpr u8 Count = 3;

} // namespace CommandQueueTypes

using CommandQueueType = CommandQueueTypes::Value;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() = 0;

    virtual void ExecuteCommandList(RHICommandList& commandList) = 0;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) = 0;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex