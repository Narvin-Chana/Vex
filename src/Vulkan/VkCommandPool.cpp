#include "VkCommandPool.h"

#include <magic_enum/magic_enum.hpp>

#include "VkCommandList.h"
#include "VkCommandQueue.h"
#include "VkErrorHandler.h"
#include "VkGPUContext.h"

namespace vex::vk
{

RHICommandList* VkCommandPool::CreateCommandList(CommandQueueType queueType)
{
    ::vk::UniqueCommandPool& commandPool = commandPoolPerQueueType[std::to_underlying(queueType)];

    auto allocatedBuffers = VEX_VK_CHECK <<= ctx.device.allocateCommandBuffersUnique({
        .commandPool = *commandPool,
        .level = ::vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    ::vk::UniqueCommandBuffer newBuffer = std::move(allocatedBuffers[0]);

    auto cmdList = MakeUnique<VkCommandList>(ctx, std::move(newBuffer), queueType);
    auto cmdListPtr = cmdList.get();

    allocatedCommandBuffers[std::to_underlying(queueType)].push_back(std::move(cmdList));

    VEX_LOG(Verbose, "Created a command list for \"{}\" type", magic_enum::enum_name(queueType));

    return cmdListPtr;
}

void VkCommandPool::ReclaimCommandListMemory(CommandQueueType queueType)
{
    VEX_LOG(Verbose,
            "Reclaimed {} command list(s) for \"{}\" type",
            allocatedCommandBuffers[std::to_underlying(queueType)].size(),
            magic_enum::enum_name(queueType));
    allocatedCommandBuffers[std::to_underlying(queueType)].clear();
}

void VkCommandPool::ReclaimAllCommandListMemory()
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        ReclaimCommandListMemory(static_cast<CommandQueueType>(i));
    }
}

VkCommandPool::VkCommandPool(VkGPUContext& ctx,
                             const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues)
    : ctx{ ctx }
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        commandPoolPerQueueType[i] = VEX_VK_CHECK <<= ctx.device.createCommandPoolUnique({
            .flags = ::vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = commandQueues[i].family,
        });
    }
}

} // namespace vex::vk
