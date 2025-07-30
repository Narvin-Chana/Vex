#pragma once

#include <Vex/RHI/RHICommandPool.h>

#include <Vulkan/VkCommandList.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkCommandPool : public RHICommandPool
{
public:
    VkCommandPool(VkGPUContext& ctx, const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues);

    virtual RHICommandList* CreateCommandList(CommandQueueType queueType) override;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) override;
    virtual void ReclaimAllCommandListMemory() override;

private:
    std::array<::vk::UniqueCommandPool, CommandQueueTypes::Count> commandPoolPerQueueType;
    std::array<std::vector<UniqueHandle<VkCommandList>>, CommandQueueTypes::Count> allocatedCommandBuffers{};
    VkGPUContext& ctx;
};
} // namespace vex::vk