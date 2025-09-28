#include "VkCommandList.h"

#include <algorithm>
#include <cmath>

#include <Vex/Bindings.h>
#include <Vex/ByteUtils.h>
#include <Vex/DrawHelpers.h>
#include <Vex/PhysicalDevice.h>

#include <RHI/RHIBindings.h>

#include <Vulkan/RHI/VkBarrier.h>
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkGraphicsPipeline.h>

namespace vex::vk
{

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

void VkCommandList::SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::SetLayout(RHIResourceLayout& layout)
{
    std::span<const byte> localConstantsData = layout.GetLocalConstantsData();
    if (localConstantsData.empty())
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
                                 localConstantsData.size(),
                                 localConstantsData.data());
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    std::array descriptorSets{ *resourceLayout.GetSamplerDescriptor().descriptorSet,
                               *descriptorPool.bindlessSet->descriptorSet };
    switch (type)
    {
    case CommandQueueTypes::Graphics:
        commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eGraphics,
                                          *resourceLayout.pipelineLayout,
                                          0,
                                          descriptorSets.size(),
                                          descriptorSets.data(),
                                          0,
                                          nullptr);
    case CommandQueueTypes::Compute:
        commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eCompute,
                                          *resourceLayout.pipelineLayout,
                                          0,
                                          descriptorSets.size(),
                                          descriptorSets.data(),
                                          0,
                                          nullptr);
        break;
    default:
        VEX_ASSERT(false, "Operation not supported on this queue type");
        break;
    }
}

void VkCommandList::SetInputAssembly(InputAssembly inputAssembly)
{
    commandBuffer->setPrimitiveRestartEnable(inputAssembly.primitiveRestartEnabled);
    commandBuffer->setPrimitiveTopology(GraphicsPiplineUtils::InputTopologyToVkTopology(inputAssembly.topology));
}

void VkCommandList::ClearTexture(const RHITextureBinding& binding,
                                 TextureUsage::Type usage,
                                 const TextureClearValue& clearValue)
{
    const ::vk::ImageSubresourceRange ranges{
        .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
        .baseMipLevel = binding.binding.subresource.startMip,
        .levelCount = binding.binding.subresource.mipCount,
        .baseArrayLayer = binding.binding.subresource.startSlice,
        .layerCount = binding.binding.subresource.sliceCount,
    };

    // const TextureDesc& desc = binding.texture->GetDesc();
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

    if (usage == TextureUsage::DepthStencil &&
        clearValue.flags & (TextureClear::ClearDepth | TextureClear::ClearStencil))
    {
        ::vk::ClearDepthStencilValue clearVal{
            .depth = clearValue.depth,
            .stencil = clearValue.stencil,
        };
        commandBuffer->clearDepthStencilImage(binding.texture->GetResource(),
                                              RHITextureLayoutToVulkan(binding.texture->GetLastLayout()),
                                              &clearVal,
                                              1,
                                              &ranges);
    }
    else
    {
        ::vk::ClearColorValue clearVal{ .float32 = clearValue.color };
        commandBuffer->clearColorImage(binding.texture->GetResource(),
                                       RHITextureLayoutToVulkan(binding.texture->GetLastLayout()),
                                       &clearVal,
                                       1,
                                       &ranges);
    }
}

void VkCommandList::Barrier(std::span<const RHIBufferBarrier> bufferBarriers,
                            std::span<const RHITextureBarrier> textureBarriers)
{
    std::vector<::vk::BufferMemoryBarrier2> vkBufferBarriers;
    vkBufferBarriers.reserve(bufferBarriers.size());
    std::vector<::vk::ImageMemoryBarrier2> vkTextureBarriers;
    vkTextureBarriers.reserve(textureBarriers.size());

    for (auto& bufferBarrier : bufferBarriers)
    {
        ::vk::BufferMemoryBarrier2 vkBarrier = {};
        vkBarrier.srcStageMask = RHIBarrierSyncToVulkan(bufferBarrier.buffer->GetLastSync());
        vkBarrier.dstStageMask = RHIBarrierSyncToVulkan(bufferBarrier.dstSync);
        vkBarrier.srcAccessMask = RHIBarrierAccessToVulkan(bufferBarrier.buffer->GetLastAccess());
        vkBarrier.dstAccessMask = RHIBarrierAccessToVulkan(bufferBarrier.dstAccess);
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.buffer = bufferBarrier.buffer->GetNativeBuffer();
        // Buffer range - for now, barrier entire buffer.
        vkBarrier.offset = 0;
        vkBarrier.size = std::numeric_limits<u64>::max();
        vkBufferBarriers.push_back(std::move(vkBarrier));

        // Update last sync and access.
        bufferBarrier.buffer->SetLastSync(bufferBarrier.dstSync);
        bufferBarrier.buffer->SetLastAccess(bufferBarrier.dstAccess);
    }
    for (auto& textureBarrier : textureBarriers)
    {
        const TextureDesc& desc = textureBarrier.texture->GetDesc();

        ::vk::ImageMemoryBarrier2 vkBarrier = {};
        if (textureBarrier.texture->IsBackBufferTexture() &&
            textureBarrier.texture->GetLastLayout() == RHITextureLayout::Undefined)
        {
            // Synchronize with vkAcquireNextImageKHR
            vkBarrier.srcStageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            vkBarrier.srcAccessMask = ::vk::AccessFlagBits2::eNone;
        }
        else
        {
            vkBarrier.srcStageMask = RHIBarrierSyncToVulkan(textureBarrier.texture->GetLastSync());
            vkBarrier.srcAccessMask = RHIBarrierAccessToVulkan(textureBarrier.texture->GetLastAccess());
        }
        vkBarrier.oldLayout = RHITextureLayoutToVulkan(textureBarrier.texture->GetLastLayout());
        vkBarrier.dstStageMask = RHIBarrierSyncToVulkan(textureBarrier.dstSync);
        vkBarrier.dstAccessMask = RHIBarrierAccessToVulkan(textureBarrier.dstAccess);
        vkBarrier.newLayout = RHITextureLayoutToVulkan(textureBarrier.dstLayout);
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.image = textureBarrier.texture->GetResource();
        vkBarrier.subresourceRange = { .aspectMask = VkTextureUtil::GetFormatAspectFlags(desc.format),
                                       .baseMipLevel = 0,
                                       .levelCount = desc.mips,
                                       .baseArrayLayer = 0,
                                       .layerCount = desc.GetSliceCount() },

        vkTextureBarriers.push_back(std::move(vkBarrier));

        // Update last sync, access and layout.
        textureBarrier.texture->SetLastSync(textureBarrier.dstSync);
        textureBarrier.texture->SetLastAccess(textureBarrier.dstAccess);
        textureBarrier.texture->SetLastLayout(textureBarrier.dstLayout);
    }

    if (vkBufferBarriers.empty() && vkTextureBarriers.empty())
    {
        return;
    }

    ::vk::DependencyInfo info{ .bufferMemoryBarrierCount = static_cast<u32>(vkBufferBarriers.size()),
                               .pBufferMemoryBarriers = vkBufferBarriers.data(),
                               .imageMemoryBarrierCount = static_cast<u32>(vkTextureBarriers.size()),
                               .pImageMemoryBarriers = vkTextureBarriers.data() };
    commandBuffer->pipelineBarrier2(info);
}

void VkCommandList::BeginRendering(const RHIDrawResources& resources)
{
    ::vk::Rect2D maxArea{ { 0, 0 }, { std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() } };
    for (auto& renderTargets : resources.renderTargets)
    {
        maxArea.extent.width = std::min(renderTargets.texture->GetDesc().width, maxArea.extent.width);
        maxArea.extent.height = std::min(renderTargets.texture->GetDesc().height, maxArea.extent.height);
    }

    std::vector<::vk::RenderingAttachmentInfo> colorAttachmentsInfo(resources.renderTargets.size());

    // Requires including the heavy <algorithm>
    std::ranges::transform(
        resources.renderTargets,
        colorAttachmentsInfo.begin(),
        [](const RHITextureBinding& rtBindings)
        {
            return ::vk::RenderingAttachmentInfo{
                .imageView = rtBindings.texture->GetOrCreateImageView(rtBindings.binding, TextureUsage::RenderTarget),
                .imageLayout = RHITextureLayoutToVulkan(rtBindings.texture->GetLastLayout()),
            };
        });

    std::optional<::vk::RenderingAttachmentInfo> depthInfo;
    if (resources.depthStencil && resources.depthStencil->texture->GetDesc().usage & TextureUsage::DepthStencil)
    {
        depthInfo = ::vk::RenderingAttachmentInfo{
            .imageView = resources.depthStencil->texture->GetOrCreateImageView(resources.depthStencil->binding,
                                                                               TextureUsage::DepthStencil),
            .imageLayout = RHITextureLayoutToVulkan(resources.depthStencil->texture->GetLastLayout()),
        };
    };

    const ::vk::RenderingInfo info{
        .renderArea = maxArea,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentsInfo.size()),
        .pColorAttachments = colorAttachmentsInfo.data(),
        .pDepthAttachment = depthInfo ? &*depthInfo : nullptr,
        .pStencilAttachment = depthInfo && DoesFormatSupportStencil(resources.depthStencil->texture->GetDesc().format)
                                  ? &*depthInfo
                                  : nullptr,
    };

    commandBuffer->beginRendering(info);
    isRendering = true;
}

void VkCommandList::EndRendering()
{
    isRendering = false;
    commandBuffer->endRendering();
}

void VkCommandList::Draw(u32 vertexCount, u32 instanceCount, u32 vertexOffset, u32 instanceOffset)
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
    commandBuffer->draw(vertexCount, instanceCount, vertexOffset, instanceOffset);
}

void VkCommandList::DrawIndexed(
    u32 indexCount, u32 instanceCount, u32 indexOffset, u32 vertexOffset, u32 instanceOffset)
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
    commandBuffer->drawIndexed(indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
}

void VkCommandList::SetVertexBuffers(u32 startSlot, std::span<RHIBufferBinding> vertexBuffers)
{
    std::vector<::vk::Buffer> vkBuffers;
    vkBuffers.reserve(vertexBuffers.size());
    std::vector<::vk::DeviceSize> vkOffsets;
    vkOffsets.reserve(vkBuffers.size());
    for (auto& [binding, buffer] : vertexBuffers)
    {
        vkBuffers.emplace_back(buffer->GetNativeBuffer());
        vkOffsets.push_back(binding.offsetByteSize.value_or(0));
    }

    commandBuffer->bindVertexBuffers(startSlot, static_cast<u32>(vkBuffers.size()), vkBuffers.data(), vkOffsets.data());
}

void VkCommandList::SetIndexBuffer(const RHIBufferBinding& indexBuffer)
{
    ::vk::IndexType indexType;
    switch (indexBuffer.binding.strideByteSize.value_or(0))
    {
    case 2:
        indexType = ::vk::IndexType::eUint16;
        break;
    case 4:
        indexType = ::vk::IndexType::eUint32;
        break;
    default:
        VEX_LOG(Fatal,
                "Unsupported index buffer stride byte size: {}. Vex only supports 2 and 4 byte indices.",
                indexBuffer.binding.strideByteSize.value_or(0));
    }

    commandBuffer->bindIndexBuffer(indexBuffer.buffer->GetNativeBuffer(),
                                   indexBuffer.binding.offsetByteSize.value_or(0),
                                   indexType);
}

void VkCommandList::Dispatch(const std::array<u32, 3>& groupCount)
{
    commandBuffer->dispatch(groupCount[0], groupCount[1], groupCount[2]);
}

void VkCommandList::TraceRays(const std::array<u32, 3>& widthHeightDepth,
                              const RHIRayTracingPipelineState& rayTracingPipelineState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::GenerateMips(RHITexture& texture)
{
    if (auto featureChecker = reinterpret_cast<VkFeatureChecker&>(*GPhysicalDevice->featureChecker);
        !featureChecker.DoesTextureFormatSupportLinearFiltering(texture.GetDesc().format))
    {
        VEX_LOG(Fatal,
                "Cannot generate mips for texture: {}! The format {} does not support linear image filtering!",
                texture.GetDesc().name,
                magic_enum::enum_name(texture.GetDesc().format));
    }

    bool isDepthStencilFormat = FormatIsDepthStencilCompatible(texture.GetDesc().format);
    ::vk::ImageAspectFlags aspectMask =
        isDepthStencilFormat ? ::vk::ImageAspectFlagBits::eDepth : ::vk::ImageAspectFlagBits::eColor;

    // Transition all mips except the first to transferDst.
    ::vk::ImageMemoryBarrier2 barrier{
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.GetResource(),
    };
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = texture.GetDesc().GetSliceCount();
    barrier.subresourceRange.baseMipLevel = 1;
    barrier.subresourceRange.levelCount = texture.GetDesc().mips - 1;
    barrier.srcStageMask = RHIBarrierSyncToVulkan(texture.GetLastSync());
    barrier.srcAccessMask = RHIBarrierAccessToVulkan(texture.GetLastAccess());
    barrier.oldLayout = RHITextureLayoutToVulkan(texture.GetLastLayout());
    barrier.dstStageMask = ::vk::PipelineStageFlagBits2::eTransfer;
    barrier.dstAccessMask = ::vk::AccessFlagBits2::eTransferWrite;
    barrier.newLayout = ::vk::ImageLayout::eTransferDstOptimal;

    ::vk::DependencyInfo info{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
    commandBuffer->pipelineBarrier2(&info);

    i32 mipWidth = texture.GetDesc().width;
    i32 mipHeight = texture.GetDesc().height;
    i32 mipDepth = texture.GetDesc().GetDepth();

    for (u16 i = 1; i < texture.GetDesc().mips; ++i)
    {
        // Transition the (i-1)th mip to transferSrc.
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.subresourceRange.levelCount = 1;
        // First mip is still in the texture's original state.
        if (i == 1)
        {
            barrier.srcStageMask = RHIBarrierSyncToVulkan(texture.GetLastSync());
            barrier.srcAccessMask = RHIBarrierAccessToVulkan(texture.GetLastAccess());
            barrier.oldLayout = RHITextureLayoutToVulkan(texture.GetLastLayout());
        }
        else
        {
            // All other mips are in TransferDst.
            barrier.srcStageMask = ::vk::PipelineStageFlagBits2::eBlit;
            barrier.srcAccessMask = ::vk::AccessFlagBits2::eTransferWrite;
            barrier.oldLayout = ::vk::ImageLayout::eTransferDstOptimal;
        }
        barrier.dstStageMask = ::vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstAccessMask = ::vk::AccessFlagBits2::eTransferRead;
        barrier.newLayout = ::vk::ImageLayout::eTransferSrcOptimal;
        commandBuffer->pipelineBarrier2(&info);

        ::vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = aspectMask;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = texture.GetDesc().GetSliceCount();
        blit.srcOffsets[0] = ::vk::Offset3D{ 0, 0, 0 };
        blit.srcOffsets[1] = ::vk::Offset3D{ mipWidth, mipHeight, mipDepth };

        blit.dstSubresource.aspectMask = aspectMask;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = texture.GetDesc().GetSliceCount();
        blit.dstOffsets[0] = ::vk::Offset3D{ 0, 0, 0 };
        blit.dstOffsets[1] = ::vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1,
                                             mipHeight > 1 ? mipHeight / 2 : 1,
                                             mipDepth > 1 ? mipDepth / 2 : 1 };

        commandBuffer->blitImage(texture.GetResource(),
                                 ::vk::ImageLayout::eTransferSrcOptimal,
                                 texture.GetResource(),
                                 ::vk::ImageLayout::eTransferDstOptimal,
                                 blit,
                                 ::vk::Filter::eLinear);

        mipWidth = std::max(1u, mipWidth / 2u);
        mipHeight = std::max(1u, mipHeight / 2u);
        mipDepth = std::max(1u, mipDepth / 2u);
    }

    // Transition the last mip to keep uniform states across the entire resource.
    barrier.subresourceRange.baseMipLevel = texture.GetDesc().mips - 1;
    barrier.srcStageMask = ::vk::PipelineStageFlagBits2::eBlit;
    barrier.srcAccessMask = ::vk::AccessFlagBits2::eTransferWrite;
    barrier.oldLayout = ::vk::ImageLayout::eTransferDstOptimal;
    barrier.dstStageMask = ::vk::PipelineStageFlagBits2::eTransfer;
    barrier.dstAccessMask = ::vk::AccessFlagBits2::eTransferRead;
    barrier.newLayout = ::vk::ImageLayout::eTransferSrcOptimal;
    commandBuffer->pipelineBarrier2(&info);

    texture.SetLastSync(RHIBarrierSync::Copy);
    texture.SetLastAccess(RHIBarrierAccess::CopySource);
    texture.SetLastLayout(RHITextureLayout::CopySource);
}

void VkCommandList::Copy(RHITexture& src, RHITexture& dst, std::span<const TextureCopyDesc> textureCopyDescriptions)
{
    const auto& srcDesc = src.desc;
    const auto& dstDesc = dst.desc;

    std::vector<::vk::ImageCopy> copyRegions{};

    const ::vk::ImageAspectFlags srcAspectMask = VkTextureUtil::GetFormatAspectFlags(srcDesc.format);
    const ::vk::ImageAspectFlags dstAspectMask = VkTextureUtil::GetFormatAspectFlags(dstDesc.format);

    copyRegions.reserve(textureCopyDescriptions.size());
    for (const auto& [srcRegion, dstRegion] : textureCopyDescriptions)
    {
        copyRegions.push_back(::vk::ImageCopy{
            .srcSubresource = { .aspectMask = srcAspectMask,
                                .mipLevel = srcRegion.subresource.startMip,
                                .baseArrayLayer = srcRegion.subresource.startSlice,
                                .layerCount = srcRegion.subresource.sliceCount },
            .srcOffset = ::vk::Offset3D(srcRegion.offset.width, srcRegion.offset.height, srcRegion.offset.depth),
            .dstSubresource = { .aspectMask = dstAspectMask,
                                .mipLevel = dstRegion.subresource.startMip,
                                .baseArrayLayer = dstRegion.subresource.startSlice,
                                .layerCount = dstRegion.subresource.sliceCount },
            .dstOffset = ::vk::Offset3D(dstRegion.offset.width, dstRegion.offset.height, dstRegion.offset.depth),
            .extent = ::vk::Extent3D{ dstRegion.extent.width, dstRegion.extent.height, dstRegion.extent.depth } });
    }

    commandBuffer->copyImage(src.GetResource(),
                             ::vk::ImageLayout::eTransferSrcOptimal,
                             dst.GetResource(),
                             ::vk::ImageLayout::eTransferDstOptimal,
                             static_cast<u32>(copyRegions.size()),
                             copyRegions.data());
}

void VkCommandList::Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDescription)
{
    const ::vk::BufferCopy copy{ .srcOffset = bufferCopyDescription.srcOffset,
                                 .dstOffset = bufferCopyDescription.dstOffset,
                                 .size = bufferCopyDescription.size };
    commandBuffer->copyBuffer(src.GetNativeBuffer(), dst.GetNativeBuffer(), 1, &copy);
}

void VkCommandList::Copy(RHIBuffer& src,
                         RHITexture& dst,
                         std::span<const BufferToTextureCopyDesc> bufferToTextureCopyDescriptions)
{
    const ::vk::ImageAspectFlags dstAspectMask =
        FormatIsDepthStencilCompatible(dst.desc.format)
            ? ::vk::ImageAspectFlagBits::eDepth | ::vk::ImageAspectFlagBits::eStencil
            : ::vk::ImageAspectFlagBits::eColor;

    float pixelByteSize = TextureUtil::GetPixelByteSizeFromFormat(dst.GetDesc().format);

    std::vector<::vk::BufferImageCopy> regions;
    regions.reserve(bufferToTextureCopyDescriptions.size());
    for (const auto& [srcRegion, dstRegion] : bufferToTextureCopyDescriptions)
    {
        u32 alignedRowPitch =
            static_cast<u32>(AlignUp<u64>(dstRegion.extent.width * pixelByteSize, TextureUtil::RowPitchAlignment));

        regions.push_back(::vk::BufferImageCopy{
            .bufferOffset = srcRegion.offset,
            // Buffer row length is in pixels.
            .bufferRowLength = static_cast<u32>(alignedRowPitch / pixelByteSize),
            .bufferImageHeight = 0,
            .imageSubresource =
                ::vk::ImageSubresourceLayers{
                    .aspectMask = dstAspectMask,
                    .mipLevel = dstRegion.subresource.startMip,
                    .baseArrayLayer = dstRegion.subresource.startSlice,
                    .layerCount = dstRegion.subresource.sliceCount,
                },
            .imageOffset = ::vk::Offset3D(dstRegion.offset.width, dstRegion.offset.height, dstRegion.offset.depth),
            .imageExtent = { dstRegion.extent.width, dstRegion.extent.height, dstRegion.extent.depth } });
    }

    commandBuffer->copyBufferToImage(src.GetNativeBuffer(),
                                     dst.GetResource(),
                                     RHITextureLayoutToVulkan(dst.GetLastLayout()),
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}

VkCommandList::VkCommandList(NonNullPtr<VkGPUContext> ctx,
                             ::vk::UniqueCommandBuffer&& commandBuffer,
                             CommandQueueType type)
    : RHICommandListBase{ type }
    , ctx{ ctx }
    , commandBuffer{ std::move(commandBuffer) }
{
}

} // namespace vex::vk
