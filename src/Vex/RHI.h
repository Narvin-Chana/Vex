#pragma once

#include <vector>

#include <Vex/EnumFlags.h>
#include <Vex/PhysicalDevice.h>

namespace vex
{

enum class CommandQueueType : u8
{
    // Transfer-only operations
    Copy,
    // Compute operations (includes Copy capabilites)
    Compute,
    // Graphics operations (includes Compute and Copy capabilities)
    Graphics,
};

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    // TODO: implement command list creation and submission. Should create a command list (=buffer) for the passed in
    // queue type. In VK, it is possible for the device to only support a graphics queue. In this case the queueType
    // should be overriden (and be opaque to the user, maybe could inform them via a warning if needed)! In DX12, all
    // queues types are creatable no matter what, so this will always return a command list on the correct queue.
    virtual void* CreateCommandList(CommandQueueType queueType)
    {
        return nullptr;
    }
    virtual void ExecuteCommandList(void* commandList)
    {
    }
};

using RHI = RenderHardwareInterface;

} // namespace vex