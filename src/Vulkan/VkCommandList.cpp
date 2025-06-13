#include "VkCommandList.h"

#include <Vex/Bindings.h>
#include <Vex/RHI/RHIBindings.h>

#include <numeric>

#include "VkDescriptorPool.h"
#include "VkErrorHandler.h"
#include "VkPipelineState.h"
#include "VkResourceLayout.h"
#include "VkTexture.h"

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
    auto& vkPSO = reinterpret_cast<const VkComputePipelineState&>(computePipelineState);
    commandBuffer->bindPipeline(::vk::PipelineBindPoint::eCompute, *vkPSO.computePipeline);
}

void VkCommandList::SetLayout(RHIResourceLayout& layout)
{
    // nothing to do here i think
}

void VkCommandList::SetLayoutLocalConstants(const RHIResourceLayout& layout, std::span<const ConstantBinding> constants)
{
    auto& vkLayout = reinterpret_cast<const VkResourceLayout&>(layout);

    std::vector<u8> pushConstantData(layout.GetMaxLocalConstantSize());

    const u32 total = std::accumulate(constants.begin(),
                                      constants.end(),
                                      0u,
                                      [](u32 acc, const ConstantBinding& binding) { return acc + binding.size; });

    VEX_ASSERT(total <= pushConstantData.size(),
               "Unable to bind local constants, you have surpassed the limit DX12 allows for in root signatures.");

    u8 currentIndex = 0;
    for (const auto& binding : constants)
    {
        std::uninitialized_copy_n(static_cast<u8*>(binding.data), binding.size, &pushConstantData[currentIndex]);
        currentIndex += binding.size;
    }

    ::vk::ShaderStageFlags stageFlags;
    switch (type)
    {
    case CommandQueueTypes::Compute:
        stageFlags = ::vk::ShaderStageFlagBits::eCompute;
        break;
    case CommandQueueTypes::Graphics:
        stageFlags = ::vk::ShaderStageFlagBits::eAllGraphics;
        break;
    default:
        VEX_ASSERT(false, "Operation not supported on this queue type");
    }

    commandBuffer->pushConstants(*vkLayout.pipelineLayout,
                                 stageFlags,
                                 0,
                                 pushConstantData.size(),
                                 pushConstantData.data());
}

void VkCommandList::SetLayoutResources(const RHIResourceLayout& layout,
                                       std::span<RHITextureBinding> textures,
                                       std::span<RHIBufferBinding> buffers,
                                       RHIDescriptorPool& descriptorPool)
{
    // VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    auto& descPool = reinterpret_cast<VkDescriptorPool&>(descriptorPool);
    auto& vkLayout = reinterpret_cast<const VkResourceLayout&>(resourceLayout);

    std::optional<::vk::PipelineBindPoint> pipelineBindPoint;
    switch (type)
    {
    case CommandQueueType::Compute:
        pipelineBindPoint = ::vk::PipelineBindPoint::eCompute;
        break;
    case CommandQueueType::Graphics:
        pipelineBindPoint = ::vk::PipelineBindPoint::eGraphics;
        break;
    default:
        pipelineBindPoint = {};
    }

    if (pipelineBindPoint)
    {
        commandBuffer->bindDescriptorSets(*pipelineBindPoint,
                                          *vkLayout.pipelineLayout,
                                          0,
                                          1,
                                          &*descPool.bindlessSet,
                                          0,
                                          nullptr);
    }
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
    commandBuffer->dispatch(groupCount[0], groupCount[1], groupCount[2]);
}

void VkCommandList::Copy(RHITexture& src, RHITexture& dst)
{
    auto& vkSrc = reinterpret_cast<VkTexture&>(src);
    auto& vkDst = reinterpret_cast<VkTexture&>(dst);

    // We assume a copy from 0,0 in the source to 0,0 in the dest with extent the size of the source
    auto& srcDesc = vkSrc.GetDescription();
    auto& dstDesc = vkSrc.GetDescription();

    VEX_ASSERT(srcDesc.depthOrArraySize <= dstDesc.depthOrArraySize);
    VEX_ASSERT(srcDesc.mips <= dstDesc.mips);
    VEX_ASSERT(srcDesc.width <= dstDesc.width);
    VEX_ASSERT(srcDesc.height <= dstDesc.height);
    VEX_ASSERT(srcDesc.type == dstDesc.type);

    ::vk::ImageLayout prevSrcLayout = vkSrc.GetLayout();
    ::vk::ImageLayout prevDstLayout = vkDst.GetLayout();

    Transition(vkSrc, ::vk::ImageLayout::eTransferSrcOptimal);
    Transition(vkDst, ::vk::ImageLayout::eTransferDstOptimal);

    std::vector<::vk::ImageCopy> copyRegions{};

    u32 width = srcDesc.width;
    u32 height = srcDesc.height;
    copyRegions.reserve(srcDesc.mips);
    for (u32 i = 0; i < srcDesc.mips; ++i)
    {
        ::vk::ImageSubresourceLayers subresource{ .aspectMask = ::vk::ImageAspectFlagBits::eColor,
                                                  .mipLevel = i,
                                                  .baseArrayLayer = 0,
                                                  .layerCount = srcDesc.type == TextureType::Texture3D
                                                                    ? srcDesc.depthOrArraySize
                                                                    : 1 };

        copyRegions.emplace_back(::vk::ImageCopy{
            .srcSubresource = subresource,
            .srcOffset = {},
            .dstSubresource = subresource,
            .dstOffset = {},
            .extent = ::vk::Extent3D{ width,
                                      height,
                                      srcDesc.type != TextureType::Texture3D ? srcDesc.depthOrArraySize : 1 } });

        width /= 2;
        height /= 2;
    }

    commandBuffer->copyImage(vkSrc.GetResource(),
                             ::vk::ImageLayout::eTransferSrcOptimal,
                             vkDst.GetResource(),
                             ::vk::ImageLayout::eTransferDstOptimal,
                             static_cast<u32>(copyRegions.size()),
                             copyRegions.data());

    Transition(vkSrc, prevSrcLayout);
    Transition(vkDst, prevDstLayout);
}

void VkCommandList::Transition(VkTexture& src, ::vk::ImageLayout targetLayout)
{
    ::vk::ImageLayout imageLayout = src.GetLayout();

    ::vk::ImageMemoryBarrier barrier{
        .srcAccessMask = ::vk::AccessFlagBits::eNone,
        .dstAccessMask = ::vk::AccessFlagBits::eNone,
        .oldLayout = imageLayout,
        .newLayout = targetLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *src.image,
        .subresourceRange = {
            .aspectMask = ::vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    using namespace ::vk;

    PipelineStageFlags sourceStage;
    switch (imageLayout)
    {
    case ImageLayout::eUndefined:
        barrier.srcAccessMask = AccessFlagBits::eNone;
        sourceStage = PipelineStageFlagBits::eTopOfPipe;
        break;
    case ImageLayout::eTransferDstOptimal:
        barrier.srcAccessMask = AccessFlagBits::eTransferWrite;
        sourceStage = PipelineStageFlagBits::eTransfer;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.srcAccessMask = AccessFlagBits::eTransferRead;
        sourceStage = PipelineStageFlagBits::eTransfer;
        break;
    default:
        VEX_ASSERT(false, "Transition source image layout not supported");
    }

    PipelineStageFlags destStage;
    switch (targetLayout)
    {
    case ImageLayout::eTransferDstOptimal:
        barrier.dstAccessMask = AccessFlagBits::eTransferWrite;
        destStage = PipelineStageFlagBits::eTransfer;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.dstAccessMask = AccessFlagBits::eTransferRead;
        destStage = PipelineStageFlagBits::eTransfer;
        break;
    case ImageLayout::eDepthStencilAttachmentOptimal:
        barrier.dstAccessMask =
            AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
        destStage = PipelineStageFlagBits::eEarlyFragmentTests;
        break;
    case ImageLayout::eShaderReadOnlyOptimal:
        barrier.dstAccessMask = AccessFlagBits::eShaderRead;
        destStage = PipelineStageFlagBits::eFragmentShader | PipelineStageFlagBits::eComputeShader;
        break;
    default:
        VEX_ASSERT(false, "Transition source image layout not supported");
    }

    commandBuffer->pipelineBarrier(sourceStage, destStage, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    src.imageLayout = targetLayout;
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
