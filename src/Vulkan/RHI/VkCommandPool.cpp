#include "VkCommandPool.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIFence.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

VkCommandPool::VkCommandPool(RHI& rhi,
                             NonNullPtr<VkGPUContext> ctx,
                             const std::array<VkCommandQueue, QueueTypes::Count>& commandQueues)
    : RHICommandPoolBase{ rhi }
    , ctx{ ctx }
{
    for (u8 i = 0; i < QueueTypes::Count; ++i)
    {
        commandPoolPerQueue[i] = VEX_VK_CHECK <<= ctx->device.createCommandPoolUnique({
            .flags = ::vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = commandQueues[i].family,
        });
    }
}

VkCommandPool::~VkCommandPool()
{
    // Reset our parent class's command buffers BEFORE destroying command pools.
    commandListsPerQueue = {};
    // Command pool will now be destroyed by class destructor.
}

NonNullPtr<RHICommandList> VkCommandPool::GetOrCreateCommandList(QueueType queueType)
{
    VkCommandList* cmdListPtr = nullptr;

    auto& commandLists = GetCommandLists(queueType);
    if (auto res = std::find_if(commandLists.begin(),
                                commandLists.end(),
                                [](const UniqueHandle<VkCommandList>& cmdList)
                                { return cmdList->GetState() == RHICommandListState::Available; });
        res != commandLists.end())
    {
        cmdListPtr = res->get();
    }
    else
    {
        ::vk::UniqueCommandPool& commandPool = GetCommandPool(queueType);
        auto allocatedBuffers = VEX_VK_CHECK <<= ctx->device.allocateCommandBuffersUnique({
            .commandPool = *commandPool,
            .level = ::vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        ::vk::UniqueCommandBuffer newBuffer = std::move(allocatedBuffers[0]);

        commandLists.push_back(MakeUnique<VkCommandList>(ctx, std::move(newBuffer), queueType));
        cmdListPtr = commandLists.back().get();
        VEX_LOG(Verbose, "Created new commandlist for queue {}", magic_enum::enum_name(queueType));
    }

    VEX_ASSERT(cmdListPtr != nullptr);
    cmdListPtr->SetState(RHICommandListState::Recording);

    return NonNullPtr(cmdListPtr);
}

::vk::UniqueCommandPool& VkCommandPool::GetCommandPool(QueueType queueType)
{
    return commandPoolPerQueue[queueType];
}

} // namespace vex::vk
