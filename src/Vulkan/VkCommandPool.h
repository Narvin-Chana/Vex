#pragma once
#include "Vex/RHI/RHICommandPool.h"
#include "VkCommandList.h"
#include "VkCommandQueue.h"

#include "VkHeaders.h"

namespace vex::vk
{
class VkCommandPool : public RHICommandPool
{
public:
    VkCommandPool(::vk::Device device, const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues);

    virtual RHICommandList* CreateCommandList(CommandQueueType queueType);
    virtual void ReclaimCommandListMemory(CommandQueueType queueType);
    virtual void ReclaimAllCommandListMemory();

private:
    std::array<::vk::UniqueCommandPool, CommandQueueTypes::Count> commandPoolPerQueueType;
    std::array<std::vector<UniqueHandle<VkCommandList>>, CommandQueueTypes::Count> allocatedCommandBuffers{};
    ::vk::Device device;
};
} // namespace vex::vk