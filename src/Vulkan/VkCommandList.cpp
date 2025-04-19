#include "VkCommandList.h"

#include <Vex/Bindings.h>

#include "VkErrorHandler.h"

namespace vex::vk
{

bool VkCommandList::IsOpen() const
{
    return isOpen;
}

void VkCommandList::Open()
{
    if (isOpen)
    {
        VEX_LOG(Fatal, "Attempting to open an already open command list.");
        return;
    }

    VEX_VK_CHECK << commandBuffer->reset();

    ::vk::CommandBufferBeginInfo beginInfo{};
    VEX_VK_CHECK << commandBuffer->begin(beginInfo);

    isOpen = true;
}

void VkCommandList::Close()
{
    if (!isOpen)
    {
        VEX_LOG(Fatal, "Attempting to close an already closed command list.");
        return;
    }

    VEX_VK_CHECK << commandBuffer->end();

    isOpen = false;
}

void VkCommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ::vk::Viewport viewport{
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .minDepth = minDepth,
        .maxDepth = maxDepth,
    };

    VEX_ASSERT(commandBuffer, "CommandBuffer must exist to set viewport");
    commandBuffer->setViewport(0, 1, &viewport);
}

void VkCommandList::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    // Special behavior to match behavior of DX12 and other APIs
    ::vk::Rect2D scissor{
        .offset = { .x = x, .y = y + static_cast<i32>(height) },
        .extent = { .width = width, .height = -height },
    };

    VEX_ASSERT(commandBuffer, "CommandBuffer must exist to set scissor");
    commandBuffer->setScissor(0, 1, &scissor);
}

void VkCommandList::SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetPipelineState(const RHIComputePipelineState& computePipelineState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetLayout(RHIResourceLayout& layout)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetLayoutLocalConstants(const RHIResourceLayout& layout, std::span<const ConstantBinding> constants)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Dispatch(const std::array<u32, 3>& groupCount, RHIResourceLayout& layout, RHITexture& backbuffer)
{
    VEX_NOT_YET_IMPLEMENTED();
}

CommandQueueType VkCommandList::GetType() const
{
    return type;
}

VkCommandList::VkCommandList(::vk::UniqueCommandBuffer&& commandBuffer, CommandQueueType type)
    : commandBuffer{ std::move(commandBuffer) }
    , type{ type }
{
}

} // namespace vex::vk
