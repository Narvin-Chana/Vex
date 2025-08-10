#include "VkCommandList.h"

#include <Vex/Bindings.h>
#include <Vex/Containers/StdContainers.h>
#include <Vex/DrawHelpers.h>
#include <Vex/RHIBindings.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkGraphicsPipeline.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkErrorHandler.h>

#include <Vex/Containers/StdContainers.h>
#include <Vex/DrawHelpers.h>

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
    cachedViewport = {
        .x = x,
        .y = y + height,
        .width = width,
        .height = -height,
        .minDepth = minDepth,
        .maxDepth = maxDepth,
    };
}

void VkCommandList::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    cachedScissor = {
        .offset = { .x = x, .y = y },
        .extent = { .width = width, .height = height },
    };
}

void VkCommandList::SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState)
{
    commandBuffer->bindPipeline(::vk::PipelineBindPoint::eGraphics, *graphicsPipelineState.graphicsPipeline);
}

void VkCommandList::SetPipelineState(const RHIComputePipelineState& computePipelineState)
{
    commandBuffer->bindPipeline(::vk::PipelineBindPoint::eCompute, *computePipelineState.computePipeline);
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

    commandBuffer->pushConstants(*layout.pipelineLayout,
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

    std::vector<u32> bindlessHandleIndices;
    bindlessHandleIndices.reserve(textures.size() + buffers.size());

    for (auto& [binding, usage, texture] : textures)
    {
        if (usage & TextureUsage::ShaderRead || usage & TextureUsage::ShaderReadWrite)
        {
            const BindlessHandle handle = texture->GetOrCreateBindlessView(binding, usage, descriptorPool);
            bindlessHandleIndices.push_back(handle.GetIndex());
        }
    }

    for (auto& [binding, buffer] : buffers)
    {
        const BindlessHandle handle =
            buffer->GetOrCreateBindlessView(binding.bufferUsage, binding.bufferStride, descriptorPool);
        bindlessHandleIndices.push_back(handle.GetIndex());
    }

    if (bindlessHandleIndices.empty())
    {
        return;
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

    commandBuffer->pushConstants(*layout.pipelineLayout,
                                 stageFlags,
                                 0,
                                 bindlessHandleIndices.size() * sizeof(u32),
                                 bindlessHandleIndices.data());
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eCompute,
                                      *resourceLayout.pipelineLayout,
                                      0,
                                      1,
                                      &*descriptorPool.bindlessSet,
                                      0,
                                      nullptr);
    commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eGraphics,
                                      *resourceLayout.pipelineLayout,
                                      0,
                                      1,
                                      &*descriptorPool.bindlessSet,
                                      0,
                                      nullptr);
}

void VkCommandList::SetInputAssembly(InputAssembly inputAssembly)
{
    commandBuffer->setPrimitiveRestartEnable(inputAssembly.primitiveRestartEnabled);
    commandBuffer->setPrimitiveTopology(GraphicsPiplineUtils::InputTopologyToVkTopology(inputAssembly.topology));
}

void VkCommandList::ClearTexture(RHITexture& rhiTexture,
                                 const ResourceBinding& clearBinding,
                                 const TextureClearValue& clearValue)
{
    const TextureDescription& desc = rhiTexture.GetDescription();

    u32 layerCount = clearBinding.sliceCount == 0 ? desc.depthOrArraySize : clearBinding.sliceCount;
    u32 mipCount = clearBinding.mipCount == 0 ? desc.mips : clearBinding.mipCount;
    //
    // if (desc.usage & TextureUsage::RenderTarget && clearValue.flags & TextureClear::ClearColor)
    // {
    //     ::vk::ClearAttachment clearAttachment{
    //         .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
    //         .colorAttachment = 0, // TODO: replace with correct attachment index?
    //         .clearValue{
    //             .color = ::vk::ClearColorValue{
    //                 .float32 = clearValue.color
    //             }
    //         }
    //     };
    //
    //     ::vk::ClearRect clearRect{
    //         .rect = ::vk::Rect2D{
    //             .offset = ::vk::Offset2D{ 0, 0 },
    //             .extent = ::vk::Extent2D{ desc.width, desc.height }
    //         },
    //         .baseArrayLayer = clearBinding.startSlice,
    //         .layerCount = layerCount,
    //     };
    //
    //     commandBuffer->clearAttachments(1, &clearAttachment, 1, &clearRect);
    // }
    // else

    if (desc.usage & TextureUsage::DepthStencil &&
        clearValue.flags & (TextureClear::ClearDepth | TextureClear::ClearStencil))
    {
        ::vk::ClearDepthStencilValue clearVal{
            .depth = clearValue.depth,
            .stencil = clearValue.stencil,
        };

        ::vk::ImageSubresourceRange ranges{
            .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
            .baseMipLevel = clearBinding.mipBias,
            .levelCount = mipCount,
            .baseArrayLayer = clearBinding.startSlice,
            .layerCount = layerCount,
        };

        commandBuffer->clearDepthStencilImage(rhiTexture.GetResource(), rhiTexture.GetLayout(), &clearVal, 1, &ranges);
    }
    else
    {
        ::vk::ClearColorValue clearVal{ .float32 = clearValue.color };

        ::vk::ImageSubresourceRange ranges{
            .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
            .baseMipLevel = clearBinding.mipBias,
            .levelCount = mipCount,
            .baseArrayLayer = clearBinding.startSlice,
            .layerCount = layerCount,
        };

        commandBuffer->clearColorImage(rhiTexture.GetResource(), rhiTexture.GetLayout(), &clearVal, 1, &ranges);
    }
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
    case ImageLayout::eColorAttachmentOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eShaderWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllGraphics | PipelineStageFlagBits2::eComputeShader;
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
    case ImageLayout::eColorAttachmentOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eMemoryWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllGraphics;
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
             .buffer = buffer.GetNativeBuffer(),
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

    auto memBarrier = GetMemoryBarrierFrom(texture, newState);

    commandBuffer->pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &memBarrier });

    texture.SetCurrentState(newState);
}

void VkCommandList::Transition(RHIBuffer& buffer, RHIBufferState::Flags newState)
{
    // Nothing to do if the states are already equal.
    if (buffer.GetCurrentState() == newState)
    {
        return;
    }

    auto memBarrier = GetBufferBarrierFrom(buffer, newState);

    commandBuffer->pipelineBarrier2({ .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &memBarrier });

    buffer.SetCurrentState(newState);
}

void VkCommandList::Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs)
{
    std::vector<::vk::ImageMemoryBarrier2> barriers;

    for (auto& [texture, flags] : textureNewStatePairs)
    {
        // Nothing to do if the states are already equal.
        if (flags == texture.GetCurrentState())
        {
            continue;
        }
        barriers.push_back(GetMemoryBarrierFrom(texture, flags));
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

    for (auto& [buffer, flags] : bufferNewStatePairs)
    {
        // Nothing to do if the states are already equal.
        if (flags == buffer.GetCurrentState())
        {
            continue;
        }
        barriers.push_back(GetBufferBarrierFrom(buffer, flags));
    }

    // No transitions means our job is done.
    if (barriers.empty())
    {
        return;
    }

    commandBuffer->pipelineBarrier2(
        { .bufferMemoryBarrierCount = static_cast<u32>(barriers.size()), .pBufferMemoryBarriers = barriers.data() });

    for (auto& [rhiTexture, flags] : bufferNewStatePairs)
    {
        rhiTexture.SetCurrentState(flags);
    }
}

void VkCommandList::BeginRendering(const RHIDrawResources& resources)
{
    ::vk::Rect2D maxArea{ { 0, 0 }, { std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() } };
    for (auto& renderTargets : resources.renderTargets)
    {
        maxArea.extent.width = std::min(renderTargets.texture->GetDescription().width, maxArea.extent.width);
        maxArea.extent.height = std::min(renderTargets.texture->GetDescription().height, maxArea.extent.height);
    }

    std::vector<::vk::RenderingAttachmentInfo> colorAttachmentsInfo(resources.renderTargets.size());
    std::ranges::transform(
        resources.renderTargets,
        colorAttachmentsInfo.begin(),
        [](const RHITextureBinding& rtBindings)
        {
            return ::vk::RenderingAttachmentInfo{
                .imageView = rtBindings.texture->GetOrCreateImageView(rtBindings.binding, TextureUsage::RenderTarget),
                .imageLayout = rtBindings.texture->GetLayout(),
            };
        });

    std::optional<::vk::RenderingAttachmentInfo> depthInfo;
    if (resources.depthStencil && resources.depthStencil->texture->GetDescription().usage & TextureUsage::DepthStencil)
    {
        depthInfo = ::vk::RenderingAttachmentInfo{
            .imageView = resources.depthStencil->texture->GetOrCreateImageView(resources.depthStencil->binding,
                                                                               TextureUsage::DepthStencil),
            .imageLayout = resources.depthStencil->texture->GetLayout(),
        };
    };

    const ::vk::RenderingInfo info{
        .renderArea = maxArea,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentsInfo.size()),
        .pColorAttachments = colorAttachmentsInfo.data(),
        .pDepthAttachment = depthInfo ? &*depthInfo : nullptr,
        .pStencilAttachment = depthInfo ? &*depthInfo : nullptr,
    };

    commandBuffer->beginRendering(info);
    isRendering = true;
}

void VkCommandList::EndRendering()
{
    isRendering = false;
    commandBuffer->endRendering();
}

void VkCommandList::Draw(u32 vertexCount)
{
    if (!cachedViewport || !cachedScissor)
    {
        VEX_LOG(Fatal, "SetScissor and SetViewport need to be called before Draw is ever called")
    }

    if (!isRendering)
    {
        VEX_LOG(Fatal, "You need to call BeginRendering before calling any draw commands")
    }

    commandBuffer->setViewportWithCount(1, &*cachedViewport);
    commandBuffer->setScissorWithCount(1, &*cachedScissor);
    commandBuffer->draw(vertexCount, 1, 0, 0);
}

void VkCommandList::Dispatch(const std::array<u32, 3>& groupCount)
{
    commandBuffer->dispatch(groupCount[0], groupCount[1], groupCount[2]);
}

void VkCommandList::Copy(RHITexture& src, RHITexture& dst)
{
    // We assume a copy from 0,0 in the source to 0,0 in the dest with extent the size of the source
    auto& srcDesc = src.GetDescription();
    auto& dstDesc = dst.GetDescription();

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

    commandBuffer->copyImage(src.GetResource(),
                             ::vk::ImageLayout::eTransferSrcOptimal,
                             dst.GetResource(),
                             ::vk::ImageLayout::eTransferDstOptimal,
                             static_cast<u32>(copyRegions.size()),
                             copyRegions.data());
}

void VkCommandList::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    const ::vk::BufferCopy copy{ .srcOffset = 0, .dstOffset = 0, .size = src.GetDescription().byteSize };
    commandBuffer->copyBuffer(src.GetNativeBuffer(), dst.GetNativeBuffer(), 1, &copy);
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
