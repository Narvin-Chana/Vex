#include "VkCommandList.h"

#include <Vex/Bindings.h>
#include <Vex/RHI/RHIBindings.h>

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
    // Manipulation to match behavior of DX12 and other APIs (this allows for hlsl shader code to work the same across
    // different apis).
    // This consists of moving the (x=0, y=0) point from the bottom-left (vk convention) to the top-left (dx, metal and
    // console convention).
    ::vk::Viewport viewport{
        .x = x,
        .y = y + height,
        .width = width,
        .height = -height,
        .minDepth = minDepth,
        .maxDepth = maxDepth,
    };

    VEX_ASSERT(commandBuffer, "CommandBuffer must exist to set viewport");
    commandBuffer->setViewport(0, 1, &viewport);
}

void VkCommandList::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    ::vk::Rect2D scissor{
        .offset = { .x = x, .y = y },
        .extent = { .width = width, .height = height },
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

void VkCommandList::SetLayoutResources(const RHIResourceLayout& layout,
                                       std::span<RHITextureBinding> textures,
                                       std::span<RHIBufferBinding> buffers,
                                       RHIDescriptorPool& descriptorPool)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Transition(RHITexture& texture, RHITextureState::Flags newState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Dispatch(const std::array<u32, 3>& groupCount)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Copy(RHITexture& src, RHITexture& dst)
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
