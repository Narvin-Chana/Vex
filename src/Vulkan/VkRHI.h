#pragma once

#include "VkCommandQueue.h"

#include <Vex/RHI/RHI.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::vk
{

class VkRHI : public vex::RHI
{
public:
    VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~VkRHI() override;

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() override;

    virtual void ExecuteCommandList(RHICommandList& commandList) override;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) override;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;

private:
    void InitWindow(const PlatformWindowHandle& windowHandle);

    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;
    ::vk::UniqueDevice device;

    std::array<VkCommandQueue, CommandQueueTypes::Count> commandQueues;
};

} // namespace vex::vk