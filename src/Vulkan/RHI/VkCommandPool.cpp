#include "VkCommandPool.h"

#include <magic_enum/magic_enum.hpp>

#include <Vulkan/RHI/VkCommandList.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

RHICommandList* VkCommandPool::CreateCommandList(CommandQueueType queueType)
{
    ::vk::UniqueCommandPool& commandPool = commandPoolPerQueueType[std::to_underlying(queueType)];

    auto allocatedBuffers = VEX_VK_CHECK <<= ctx->device.allocateCommandBuffersUnique({
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
    // Decrement lifespan of all allocated buffers.
    auto& commandBuffers = allocatedCommandBuffers[std::to_underlying(queueType)];
    VEX_LOG(Verbose,
            "Reclaimed {} command list(s) for \"{}\" type",
            commandBuffers.size(),
            magic_enum::enum_name(queueType));

    // Release underlying command buffers now that they are assuredly no longer in flight.
    commandBuffers.clear();
}

void VkCommandPool::ReclaimAllCommandListMemory()
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        ReclaimCommandListMemory(static_cast<CommandQueueType>(i));
    }
}

VkCommandPool::VkCommandPool(NonNullPtr<VkGPUContext> ctx,
                             const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues)
    : ctx{ ctx }
{
    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        commandPoolPerQueueType[i] = VEX_VK_CHECK <<= ctx->device.createCommandPoolUnique({
            // TODO(https://trello.com/c/L4xZOEBK): currently the VkCommandPool has a weird mix between reusing and not
            // reusing command buffers,
            // If we reuse buffers, we should store them and redistribute them instead of just allocating a new buffer
            // every time we request one.
            // To address this I've temporarily made it so that buffers are never reused.
            //.flags = ::vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = commandQueues[i].family,
        });
    }
}

} // namespace vex::vk
