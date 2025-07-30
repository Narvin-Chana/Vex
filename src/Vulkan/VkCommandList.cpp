#include "VkCommandList.h"

#include <Vex/Bindings.h>
#include <Vex/RHI/RHIBindings.h>

#include <Vulkan/VkBuffer.h>
#include <Vulkan/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkPipelineState.h>
#include <Vulkan/VkResourceLayout.h>
#include <Vulkan/VkTexture.h>

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

    constexpr ::vk::CommandBufferBeginInfo beginInfo{};
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
    if (constants.empty())
    {
        return;
    }

    auto constantData = ConstantBinding::ConcatConstantBindings(constants, layout.GetMaxLocalConstantSize());

    auto& vkLayout = reinterpret_cast<const VkResourceLayout&>(layout);

    ::vk::ShaderStageFlags stageFlags;
    switch (type)
    {
    case CommandQueueTypes::Graphics:
        stageFlags |= ::vk::ShaderStageFlagBits::eAllGraphics;
    case CommandQueueTypes::Compute:
        stageFlags |= ::vk::ShaderStageFlagBits::eCompute;
        break;
    default:
        VEX_ASSERT(false, "Operation not supported on this queue type");
    }

    commandBuffer->pushConstants(*vkLayout.pipelineLayout,
                                 stageFlags,
                                 0, // Local constants start at 0
                                 constantData.size(),
                                 constantData.data());
}

void VkCommandList::SetLayoutResources(const RHIResourceLayout& layout,
                                       std::span<RHITextureBinding> textures,
                                       std::span<RHIBufferBinding> buffers,
                                       RHIDescriptorPool& descriptorPool)
{
    if (textures.empty() && buffers.empty())
    {
        return;
    }

    auto& vkResourceLayout = reinterpret_cast<const VkResourceLayout&>(layout);
    auto& vkDescriptorPool = reinterpret_cast<VkDescriptorPool&>(descriptorPool);

    std::vector<u32> bindlessHandleIndices;
    bindlessHandleIndices.reserve(textures.size() + buffers.size());

    for (auto& [binding, usage, rhiTexture] : textures)
    {
        auto vkTexture = reinterpret_cast<VkTexture*>(rhiTexture);

        if (usage == TextureUsage::ShaderRead || usage == TextureUsage::ShaderReadWrite)
        {
            const BindlessHandle handle = vkTexture->GetOrCreateBindlessView(
                ctx,
                VkTextureViewDesc{
                    .viewType = TextureUtil::GetTextureViewType(binding),
                    .format = TextureUtil::GetTextureFormat(binding),
                    .usage = usage,
                    .mipBias = binding.mipBias,
                    .mipCount = (binding.mipCount == 0) ? rhiTexture->GetDescription().mips : binding.mipCount,
                    .startSlice = binding.startSlice,
                    .sliceCount =
                        (binding.sliceCount == 0) ? rhiTexture->GetDescription().depthOrArraySize : binding.sliceCount,
                },
                vkDescriptorPool);
            bindlessHandleIndices.push_back(handle.GetIndex());
        }
    }

    for (auto& [binding, usage, rhiBuffer] : buffers)
    {
        auto* vkBuffer = reinterpret_cast<VkBuffer*>(rhiBuffer);
        const BindlessHandle handle = vkBuffer->GetOrCreateBindlessIndex(ctx, vkDescriptorPool);
        bindlessHandleIndices.push_back(handle.GetIndex());
    }

    for (auto& [binding, usage, rhiBuffer] : buffers)
    {
        auto* vkBuffer = reinterpret_cast<VkBuffer*>(rhiBuffer);
        const BindlessHandle handle = vkBuffer->GetOrCreateBindlessIndex(ctx, vkDescriptorPool);
        bindlessHandleIndices.push_back(handle.GetIndex());
    }

    ::vk::ShaderStageFlags stageFlags;
    switch (type)
    {
    case CommandQueueTypes::Graphics:
        stageFlags |= ::vk::ShaderStageFlagBits::eAllGraphics;
    case CommandQueueTypes::Compute:
        stageFlags |= ::vk::ShaderStageFlagBits::eCompute;
        break;
    default:
        VEX_ASSERT(false, "Operation not supported on this queue type");
    }

    commandBuffer->pushConstants(*vkResourceLayout.pipelineLayout,
                                 stageFlags,
                                 0,
                                 bindlessHandleIndices.size() * sizeof(u32),
                                 bindlessHandleIndices.data());
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    auto& descPool = reinterpret_cast<VkDescriptorPool&>(descriptorPool);
    auto& vkLayout = reinterpret_cast<VkResourceLayout&>(resourceLayout);

    commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eCompute,
                                      *vkLayout.pipelineLayout,
                                      0,
                                      1,
                                      &*descPool.bindlessSet,
                                      0,
                                      nullptr);
    commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eGraphics,
                                      *vkLayout.pipelineLayout,
                                      0,
                                      1,
                                      &*descPool.bindlessSet,
                                      0,
                                      nullptr);
}

void VkCommandList::SetInputAssembly(InputAssembly inputAssembly)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::ClearTexture(RHITexture& rhiTexture,
                                 const ResourceBinding& clearBinding,
                                 const TextureClearValue& clearValue)
{
    VEX_NOT_YET_IMPLEMENTED();
}

// This only changes the access mask of the texture
::vk::ImageMemoryBarrier2 GetMemoryBarrierFrom(VkTexture& texture, RHITextureState::Flags flags)
{
    ::vk::ImageLayout prevLayout = texture.GetLayout();
    ::vk::ImageLayout nextLayout = TextureUtil::TextureStateFlagToImageLayout(flags);

    ::vk::ImageMemoryBarrier2 barrier{
        // not the best stage mask. Should be more precise for better sync
        .oldLayout = prevLayout,
        .newLayout = nextLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.GetResource(),
        .subresourceRange = {
            .aspectMask = ::vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    using namespace ::vk;

    switch (prevLayout)
    {
    case ImageLayout::eUndefined:
        barrier.srcAccessMask = AccessFlagBits2::eNone;
        barrier.srcStageMask = PipelineStageFlagBits2::eTopOfPipe;
        break;
    case ImageLayout::eTransferDstOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eTransferWrite;
        barrier.srcStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eTransferRead;
        barrier.srcStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eShaderReadOnlyOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eShaderRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllGraphics | PipelineStageFlagBits2::eComputeShader;
        break;
    case ImageLayout::eGeneral:
    case ImageLayout::ePresentSrcKHR:
        barrier.srcAccessMask = AccessFlagBits2::eMemoryRead | AccessFlagBits2::eMemoryWrite;
        barrier.srcStageMask = PipelineStageFlagBits2::eAllCommands;
        break;
    default:
        VEX_ASSERT(false, "Transition source image layout not supported");
    }

    switch (nextLayout)
    {
    case ImageLayout::eTransferDstOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eTransferWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eTransferRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eDepthStencilAttachmentOptimal:
        barrier.dstAccessMask =
            AccessFlagBits2::eDepthStencilAttachmentRead | AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eEarlyFragmentTests;
        break;
    case ImageLayout::eShaderReadOnlyOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eShaderRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllGraphics | PipelineStageFlagBits2::eComputeShader;
        break;
    case ImageLayout::eGeneral:
    case ImageLayout::ePresentSrcKHR:
        barrier.dstAccessMask = AccessFlagBits2::eMemoryRead | AccessFlagBits2::eMemoryWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllCommands;
        break;
    default:
        VEX_ASSERT(false, "Transition source image layout not supported");
    }

    return barrier;
}

::vk::BufferMemoryBarrier2 GetBufferBarrierFrom(VkBuffer& buffer, RHIBufferState::Flags flags)
{
    ::vk::AccessFlags2 srcAccessMask = BufferUtil::GetAccessFlagsFromBufferState(buffer.GetCurrentState());
    ::vk::AccessFlags2 dstAccessMask = BufferUtil::GetAccessFlagsFromBufferState(flags);

    // TODO: Figure out how to be more specific with stages
    return { .srcStageMask = ::vk::PipelineStageFlagBits2::eAllCommands,
             .srcAccessMask = srcAccessMask,
             .dstStageMask = ::vk::PipelineStageFlagBits2::eAllCommands,
             .dstAccessMask = dstAccessMask,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .buffer = buffer.GetBuffer(),
             .offset = 0,
             .size = buffer.GetDescription().byteSize };
}

void VkCommandList::Transition(RHITexture& texture, RHITextureState::Flags newState)
{
    // Nothing to do if the states are already equal.
    if (texture.GetCurrentState() == newState)
    {
        return;
    }

    auto& vkTexture = reinterpret_cast<VkTexture&>(texture);
    auto memBarrier = GetMemoryBarrierFrom(vkTexture, newState);

    commandBuffer->pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &memBarrier });

    texture.SetCurrentState(newState);
}

void VkCommandList::Transition(RHIBuffer& buffer, RHIBufferState::Flags newState)
{
    auto& vkBuffer = reinterpret_cast<VkBuffer&>(buffer);
    auto memBarrier = GetBufferBarrierFrom(vkBuffer, newState);

    commandBuffer->pipelineBarrier2({ .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &memBarrier });

    buffer.SetCurrentState(newState);
}

void VkCommandList::Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs)
{
    std::vector<::vk::ImageMemoryBarrier2> barriers;

    for (auto& [rhiTexture, flags] : textureNewStatePairs)
    {
        // Nothing to do if the states are already equal.
        if (flags == rhiTexture.GetCurrentState())
        {
            continue;
        }
        auto& vkTexture = reinterpret_cast<VkTexture&>(rhiTexture);
        barriers.push_back(GetMemoryBarrierFrom(vkTexture, flags));
    }

    // No transitions means our job is done.
    if (barriers.empty())
    {
        return;
    }

    commandBuffer->pipelineBarrier2(
        { .imageMemoryBarrierCount = static_cast<u32>(barriers.size()), .pImageMemoryBarriers = barriers.data() });

    for (auto& [rhiTexture, flags] : textureNewStatePairs)
    {
        rhiTexture.SetCurrentState(flags);
    }
}

void VkCommandList::Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs)
{
    std::vector<::vk::BufferMemoryBarrier2> barriers;

    for (auto& [rhiTexture, flags] : bufferNewStatePairs)
    {
        auto& vkBuffer = reinterpret_cast<VkBuffer&>(rhiTexture);
        barriers.push_back(GetBufferBarrierFrom(vkBuffer, flags));
    }

    commandBuffer->pipelineBarrier2(
        { .bufferMemoryBarrierCount = static_cast<u32>(barriers.size()), .pBufferMemoryBarriers = barriers.data() });

    for (auto& [rhiTexture, flags] : bufferNewStatePairs)
    {
        rhiTexture.SetCurrentState(flags);
    }
}

void VkCommandList::Draw(u32 vertexCount)
{
    commandBuffer->draw(vertexCount, 1, 0, 0);
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
}

void VkCommandList::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    auto& vkSrc = reinterpret_cast<VkBuffer&>(src);
    auto& vkDst = reinterpret_cast<VkBuffer&>(dst);

    const ::vk::BufferCopy copy{ .srcOffset = 0, .dstOffset = 0, .size = vkSrc.GetDescription().byteSize };

    commandBuffer->copyBuffer(vkSrc.GetBuffer(), vkDst.GetBuffer(), 1, &copy);
}

CommandQueueType VkCommandList::GetType() const
{
    return type;
}

VkCommandList::VkCommandList(VkGPUContext& ctx, ::vk::UniqueCommandBuffer&& commandBuffer, CommandQueueType type)
    : ctx{ ctx }
    , commandBuffer{ std::move(commandBuffer) }
    , type{ type }
{
}

} // namespace vex::vk
