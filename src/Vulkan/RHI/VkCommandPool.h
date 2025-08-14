#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/NonNullPtr.h>

#include <RHI/RHICommandPool.h>

#include <Vulkan/RHI/VkCommandList.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkCommandPool final : public RHICommandPoolInterface
{
public:
    VkCommandPool(NonNullPtr<VkGPUContext> ctx,
                  const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues);

    virtual RHICommandList* CreateCommandList(CommandQueueType queueType) override;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) override;
    virtual void ReclaimAllCommandListMemory() override;

private:
    NonNullPtr<VkGPUContext> ctx;
    std::array<::vk::UniqueCommandPool, CommandQueueTypes::Count> commandPoolPerQueueType;
    std::array<std::vector<UniqueHandle<VkCommandList>>, CommandQueueTypes::Count> allocatedCommandBuffers{};
};
} // namespace vex::vk