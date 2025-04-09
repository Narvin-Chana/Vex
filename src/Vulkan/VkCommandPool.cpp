#include "VkCommandPool.h"

#include "VkCommandList.h"
#include "VkCommandQueue.h"
#include "VkErrorHandler.h"

namespace vex::vk
{

RHICommandList* VkCommandPool::CreateCommandList(CommandQueueType queueType)
{
    ::vk::UniqueCommandPool& commandPool = commandPoolPerQueueType[std::to_underlying(queueType)];

    ::vk::UniqueCommandBuffer newBuffer = std::move(Sanitize(device.allocateCommandBuffersUnique({
        .commandPool = *commandPool,
        .level = ::vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    }))[0]);

    auto cmdList = MakeUnique<VkCommandList>(std::move(newBuffer), queueType);
    auto cmdListPtr = cmdList.get();

    allocatedCommandBuffers[std::to_underlying(queueType)].push_back(std::move(cmdList));

    return cmdListPtr;
}

void VkCommandPool::ReclaimCommandListMemory(CommandQueueType queueType)
{
    allocatedCommandBuffers[std::to_underlying(queueType)].clear();
}

void VkCommandPool::ReclaimAllCommandListMemory()
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        ReclaimCommandListMemory(static_cast<CommandQueueType>(i));
    }
}

VkCommandPool::VkCommandPool(::vk::Device device,
                             const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues)
    : device{ device }
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        commandPoolPerQueueType[i] = Sanitize(device.createCommandPoolUnique({
            .flags = ::vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = commandQueues[i].family,
        }));
    }
}

} // namespace vex::vk
