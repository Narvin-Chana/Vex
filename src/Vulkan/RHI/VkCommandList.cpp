#include "VkCommandList.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include <Vex/Bindings.h>
#include <Vex/DrawHelpers.h>
#include <Vex/Utility/Algorithms.h>
#include <Vex/Utility/ByteUtils.h>

#include <RHI/RHIBindings.h>
#include <RHI/RHIPhysicalDevice.h>

#include <Vulkan/RHI/VkBarrier.h>
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkScopedGPUEvent.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/RHI/VkTimestampQueryPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkGraphicsPipeline.h>

namespace vex::vk
{

namespace CommandList_Internal
{

static std::vector<::vk::BufferImageCopy> GetBufferImageCopyFromBufferToImageDescriptions(
    const RHITexture& texture, Span<const BufferTextureCopyDesc> descriptions)
{
    std::optional<RHITextureLayout> layout{};

    std::vector<::vk::BufferImageCopy> regions;
    regions.reserve(descriptions.size());
    for (const auto& [bufferRegion, textureRegion] : descriptions)
    {
        float pixelByteSize = TextureUtil::GetPixelByteSizeFromFormat(
            TextureUtil::GetCopyFormat(texture.GetDesc().format,
                                       textureRegion.subresource.GetSingleAspect(texture.GetDesc())));

        u32 alignedRowPitch = static_cast<u32>(AlignUp<u64>(
            textureRegion.extent.GetWidth(texture.GetDesc(), textureRegion.subresource.startMip) * pixelByteSize,
            TextureUtil::RowPitchAlignment));

        TextureAspect::Type aspect = textureRegion.subresource.GetSingleAspect(texture.GetDesc());

        u32 startLayer = textureRegion.subresource.startSlice;
        u32 layerCount = textureRegion.subresource.GetSliceCount(texture.GetDesc());

        regions.push_back(::vk::BufferImageCopy{
            .bufferOffset = bufferRegion.offset,
            // Buffer row length is in pixels.
            .bufferRowLength = static_cast<u32>(alignedRowPitch / pixelByteSize),
            .bufferImageHeight = 0,
            .imageSubresource =
                ::vk::ImageSubresourceLayers{
                    .aspectMask = VkTextureUtil::BindingAspectToVkAspectFlags(aspect),
                    .mipLevel = textureRegion.subresource.startMip,
                    .baseArrayLayer = startLayer,
                    .layerCount = layerCount,
                },
            .imageOffset = ::vk::Offset3D(textureRegion.offset.x, textureRegion.offset.y, textureRegion.offset.z),
            .imageExtent = {
                textureRegion.extent.GetWidth(texture.GetDesc(), textureRegion.subresource.startMip),
                textureRegion.extent.GetHeight(texture.GetDesc(), textureRegion.subresource.startMip),
                textureRegion.extent.GetDepth(texture.GetDesc(), textureRegion.subresource.startMip),
            } });
    }

    return regions;
}

} // namespace CommandList_Internal

void VkCommandList::Open()
{
    VEX_VK_CHECK << commandBuffer->reset();

    constexpr ::vk::CommandBufferBeginInfo beginInfo{};
    VEX_VK_CHECK << commandBuffer->begin(beginInfo);

    RHICommandListBase::Open();
}

void VkCommandList::Close()
{
    RHICommandListBase::Close();

    VEX_VK_CHECK << commandBuffer->end();
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
    Span<const byte> localConstantsData = layout.GetLocalConstantsData();
    if (localConstantsData.empty())
    {
        return;
    }

    // Stage flags must be the same as the push constant ranges defined in the layout
    commandBuffer->pushConstants(*layout.pipelineLayout,
                                 ::vk::ShaderStageFlagBits::eAllGraphics | ::vk::ShaderStageFlagBits::eCompute,
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
    case QueueTypes::Graphics:
        commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eGraphics,
                                          *resourceLayout.pipelineLayout,
                                          0,
                                          descriptorSets.size(),
                                          descriptorSets.data(),
                                          0,
                                          nullptr);
    case QueueTypes::Compute:
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
                                 const TextureClearValue& clearValue,
                                 Span<const TextureClearRect> clearRects)
{
    const ::vk::ImageSubresourceRange ranges{
        .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.clearAspect),
        .baseMipLevel = binding.binding.subresource.startMip,
        .levelCount = binding.binding.subresource.GetMipCount(binding.texture->GetDesc()),
        .baseArrayLayer = binding.binding.subresource.startSlice,
        .layerCount = binding.binding.subresource.GetSliceCount(binding.texture->GetDesc()),
    };

    TextureAspect::Flags aspect = 0;
    if (clearValue.clearAspect & TextureAspect::Color)
        aspect |= TextureAspect::Color;
    if (clearValue.clearAspect & TextureAspect::Depth)
        aspect |= TextureAspect::Depth;
    if (clearValue.clearAspect & TextureAspect::Stencil)
        aspect |= TextureAspect::Stencil;

    if (clearRects.empty())
    {
        if (usage == TextureUsage::DepthStencil &&
            clearValue.clearAspect & (TextureAspect::Depth | TextureAspect::Stencil))
        {
            ::vk::ClearDepthStencilValue clearVal{
                .depth = clearValue.depth,
                .stencil = clearValue.stencil,
            };
            commandBuffer->clearDepthStencilImage(binding.texture->GetResource(),
                                                  ::vk::ImageLayout::eGeneral,
                                                  &clearVal,
                                                  1,
                                                  &ranges);
        }
        else
        {
            ::vk::ClearColorValue clearVal{ .float32 = clearValue.color };
            commandBuffer->clearColorImage(binding.texture->GetResource(),
                                           ::vk::ImageLayout::eGeneral,
                                           &clearVal,
                                           1,
                                           &ranges);
        }
    }
    else
    {
        RHIDrawResources resources;

        std::vector<::vk::ClearRect> rects;
        for (auto rect : clearRects)
        {
            rects.push_back({ .rect = ::vk::Rect2D{ .offset = { rect.offsetX, rect.offsetY },
                                                    .extent = { rect.GetExtentX(binding.texture->GetDesc()),
                                                                rect.GetExtentY(binding.texture->GetDesc()) } },
                              .baseArrayLayer = ranges.baseArrayLayer,
                              .layerCount = ranges.layerCount });
        }

        ::vk::ClearAttachment clearAttachment{};
        clearAttachment.aspectMask |= (clearValue.clearAspect & TextureAspect::Depth)
                                          ? ::vk::ImageAspectFlagBits::eDepth
                                          : ::vk::ImageAspectFlagBits::eNone;
        clearAttachment.aspectMask |= (clearValue.clearAspect & TextureAspect::Stencil)
                                          ? ::vk::ImageAspectFlagBits::eStencil
                                          : ::vk::ImageAspectFlagBits::eNone;
        clearAttachment.aspectMask |= (clearValue.clearAspect & TextureAspect::Color)
                                          ? ::vk::ImageAspectFlagBits::eColor
                                          : ::vk::ImageAspectFlagBits::eNone;

        if (usage == TextureUsage::DepthStencil &&
            clearValue.clearAspect & (TextureAspect::Depth | TextureAspect::Stencil))
        {
            resources.depthStencil = binding;
            clearAttachment.clearValue.depthStencil = ::vk::ClearDepthStencilValue{
                .depth = clearValue.depth,
                .stencil = clearValue.stencil,
            };
        }
        else
        {
            resources.renderTargets.push_back(binding);
            clearAttachment.clearValue.color = ::vk::ClearColorValue{ .float32 = clearValue.color };
        }

        BeginRendering(resources);
        commandBuffer->clearAttachments(1, &clearAttachment, rects.size(), rects.data());
        EndRendering();
    }
}

void VkCommandList::EmitBarriers(Span<const RHIBufferBarrier> bufferBarriers,
                                 Span<const RHITextureBarrier> textureBarriers,
                                 Span<const RHIGlobalBarrier> globalBarriers)
{
    ::vk::PipelineStageFlags2 srcSyncMask;
    ::vk::PipelineStageFlags2 dstSyncMask;
    ::vk::AccessFlags2 srcAccessMask;
    ::vk::AccessFlags2 dstAccessMask;

    std::vector<::vk::ImageMemoryBarrier2> imageBarriers;
    imageBarriers.reserve(textureBarriers.size());

    for (const auto& tb : textureBarriers)
    {
        const TextureDesc& desc = tb.texture->GetDesc();
        // Unified Image Layouts allows us to only consider a few image transitions.
        // We only need to emit an image barrier when: eUndefined<->eGeneral<->ePresentSrcKHR
        const bool isFullResource = tb.subresource.IsFullResource(desc);
        const ::vk::ImageLayout oldLayout = RHITextureLayoutToVulkan(tb.srcLayout);
        const ::vk::ImageLayout newLayout = RHITextureLayoutToVulkan(tb.dstLayout);
        const bool needsLayoutTransition = oldLayout != newLayout;

        if (!isFullResource || needsLayoutTransition)
        {
            ::vk::ImageMemoryBarrier2 ib;
            ib.srcStageMask = (tb.texture->IsBackBufferTexture() && oldLayout == ::vk::ImageLayout::eUndefined)
                                  ? ::vk::PipelineStageFlagBits2::eColorAttachmentOutput
                                  : RHIBarrierSyncToVulkan(tb.srcSync);
            ib.srcAccessMask = RHIBarrierAccessToVulkan(tb.srcAccess);
            ib.dstStageMask = RHIBarrierSyncToVulkan(tb.dstSync);
            ib.dstAccessMask = RHIBarrierAccessToVulkan(tb.dstAccess);
            ib.oldLayout = oldLayout;
            ib.newLayout = newLayout;
            ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ib.image = tb.texture->GetResource();
            ib.subresourceRange = {
                .aspectMask = VkTextureUtil::GetFormatAspectFlags(desc.format),
                .baseMipLevel = tb.subresource.startMip,
                .levelCount = tb.subresource.GetMipCount(desc),
                .baseArrayLayer = tb.subresource.startSlice,
                .layerCount = tb.subresource.GetSliceCount(desc),
            };
            imageBarriers.push_back(std::move(ib));
        }
        else
        {
            // UIL fast path: no layout transition, just fold into memory barrier.
            srcSyncMask |= RHIBarrierSyncToVulkan(tb.srcSync);
            dstSyncMask |= RHIBarrierSyncToVulkan(tb.dstSync);
            srcAccessMask |= RHIBarrierAccessToVulkan(tb.srcAccess);
            dstAccessMask |= RHIBarrierAccessToVulkan(tb.dstAccess);
        }
    }

    // Buffers and global barriers always fold into memory barrier.
    for (const auto& bb : bufferBarriers)
    {
        srcSyncMask |= RHIBarrierSyncToVulkan(bb.srcSync);
        dstSyncMask |= RHIBarrierSyncToVulkan(bb.dstSync);
        srcAccessMask |= RHIBarrierAccessToVulkan(bb.srcAccess);
        dstAccessMask |= RHIBarrierAccessToVulkan(bb.dstAccess);
    }
    for (const auto& gb : globalBarriers)
    {
        srcSyncMask |= RHIBarrierSyncToVulkan(gb.srcSync);
        dstSyncMask |= RHIBarrierSyncToVulkan(gb.dstSync);
        srcAccessMask |= RHIBarrierAccessToVulkan(gb.srcAccess);
        dstAccessMask |= RHIBarrierAccessToVulkan(gb.dstAccess);
    }

    ::vk::MemoryBarrier2 memoryBarrier = ::vk::MemoryBarrier2()
                                             .setSrcStageMask(srcSyncMask)
                                             .setDstStageMask(dstSyncMask)
                                             .setSrcAccessMask(srcAccessMask)
                                             .setDstAccessMask(dstAccessMask);

    ::vk::DependencyInfo info{
        .memoryBarrierCount = 1u,
        .pMemoryBarriers = &memoryBarrier,
        .imageMemoryBarrierCount = static_cast<u32>(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };
    commandBuffer->pipelineBarrier2(info);
}

void VkCommandList::BeginRendering(const RHIDrawResources& resources)
{
    VEX_ASSERT(!isRendering,
               "Cannot call BeginRendering when already rendering, you must have forgotten to call EndRendering!");

    ::vk::Rect2D maxArea{ { 0, 0 }, { std::numeric_limits<u32>::max(), std::numeric_limits<u32>::max() } };
    for (auto& renderTargets : resources.renderTargets)
    {
        maxArea.extent.width = std::min(renderTargets.texture->GetDesc().width, maxArea.extent.width);
        maxArea.extent.height = std::min(renderTargets.texture->GetDesc().height, maxArea.extent.height);
    }
    if (resources.depthStencil)
    {
        maxArea.extent.width = std::min(resources.depthStencil->texture->GetDesc().width, maxArea.extent.width);
        maxArea.extent.height = std::min(resources.depthStencil->texture->GetDesc().height, maxArea.extent.height);
    }

    std::vector<::vk::RenderingAttachmentInfo> colorAttachmentsInfo(resources.renderTargets.size());

    std::ranges::transform(
        resources.renderTargets,
        colorAttachmentsInfo.begin(),
        [](const RHITextureBinding& rtBindings)
        {
            return ::vk::RenderingAttachmentInfo{
                .imageView = rtBindings.texture->GetOrCreateImageView(rtBindings.binding, TextureUsage::RenderTarget),
                .imageLayout = ::vk::ImageLayout::eGeneral,
            };
        });

    std::optional<::vk::RenderingAttachmentInfo> depthInfo;
    if (resources.depthStencil && resources.depthStencil->texture->GetDesc().usage & TextureUsage::DepthStencil)
    {
        depthInfo = ::vk::RenderingAttachmentInfo{
            .imageView = resources.depthStencil->texture->GetOrCreateImageView(resources.depthStencil->binding,
                                                                               TextureUsage::DepthStencil),
            .imageLayout = ::vk::ImageLayout::eGeneral,
        };
    };

    const ::vk::RenderingInfo info{
        .renderArea = maxArea,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(colorAttachmentsInfo.size()),
        .pColorAttachments = colorAttachmentsInfo.data(),
        .pDepthAttachment = depthInfo ? &*depthInfo : nullptr,
        .pStencilAttachment =
            depthInfo && FormatUtil::IsDepthAndStencilFormat(resources.depthStencil->texture->GetDesc().format)
                ? &*depthInfo
                : nullptr,
    };

    commandBuffer->beginRendering(info);
    isRendering = true;
}

void VkCommandList::EndRendering()
{
    VEX_ASSERT(isRendering, "Cannot call EndRendering without calling BeginRendering first.");
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

void VkCommandList::SetVertexBuffers(u32 startSlot, Span<const RHIBufferBinding> vertexBuffers)
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

void VkCommandList::TraceRays(const TraceRaysDesc& rayTracingArgs,
                              const RHIRayTracingPipelineState& rayTracingPipelineState)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::GenerateMips(RHITexture& texture, const TextureSubresource& subresource)
{
    bool isDepthStencilFormat = FormatUtil::IsDepthOrStencilFormat(texture.GetDesc().format);
    ::vk::ImageAspectFlags aspectMask =
        isDepthStencilFormat ? ::vk::ImageAspectFlagBits::eDepth : ::vk::ImageAspectFlagBits::eColor;

    u16 sourceMip = subresource.startMip;

    i32 mipWidth = std::max<i32>(1, texture.GetDesc().width >> sourceMip);
    i32 mipHeight = std::max<i32>(1, texture.GetDesc().height >> sourceMip);
    i32 mipDepth = std::max<i32>(1, texture.GetDesc().GetDepth() >> sourceMip);

    for (u16 i = sourceMip + 1; i < subresource.startMip + subresource.GetMipCount(texture.desc); ++i)
    {
        // Wait for the previous mip to be done writing to.
        EmitBarriers({},
                     {},
                     { RHIGlobalBarrier{ .srcSync = RHIBarrierSync::AllCommands,
                                         .dstSync = RHIBarrierSync::Blit,
                                         .srcAccess = RHIBarrierAccess::MemoryWrite,
                                         .dstAccess = RHIBarrierAccess::CopySource } });

        ::vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = aspectMask;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = subresource.startSlice;
        blit.srcSubresource.layerCount = subresource.GetSliceCount(texture.desc);
        blit.srcOffsets[0] = ::vk::Offset3D{ 0, 0, 0 };
        blit.srcOffsets[1] = ::vk::Offset3D{ mipWidth, mipHeight, mipDepth };

        blit.dstSubresource.aspectMask = aspectMask;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = subresource.startSlice;
        blit.dstSubresource.layerCount = subresource.GetSliceCount(texture.desc);
        blit.dstOffsets[0] = ::vk::Offset3D{ 0, 0, 0 };
        blit.dstOffsets[1] = ::vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1,
                                             mipHeight > 1 ? mipHeight / 2 : 1,
                                             mipDepth > 1 ? mipDepth / 2 : 1 };

        commandBuffer->blitImage(texture.GetResource(),
                                 ::vk::ImageLayout::eGeneral,
                                 texture.GetResource(),
                                 ::vk::ImageLayout::eGeneral,
                                 blit,
                                 ::vk::Filter::eLinear);

        mipWidth = std::max(1u, mipWidth / 2u);
        mipHeight = std::max(1u, mipHeight / 2u);
        mipDepth = std::max(1u, mipDepth / 2u);
    }

    EmitBarriers({},
                 {},
                 { RHIGlobalBarrier{
                     .srcSync = RHIBarrierSync::Blit,
                     .dstSync = RHIBarrierSync::AllCommands,
                     .srcAccess = RHIBarrierAccess::MemoryWrite,
                     .dstAccess = RHIBarrierAccess::MemoryRead,
                 } });
}

QueryHandle VkCommandList::BeginTimestampQuery()
{
    VEX_ASSERT(queryPool);

    QueryHandle handle = queryPool->AllocateQuery(GetType());

    commandBuffer->resetQueryPool(queryPool->GetNativeQueryPool(), handle.GetIndex() * 2, 2);
    commandBuffer->writeTimestamp(::vk::PipelineStageFlagBits::eTopOfPipe,
                                  queryPool->GetNativeQueryPool(),
                                  handle.GetIndex() * 2);
    queries.push_back(handle);
    return handle;
}

void VkCommandList::EndTimestampQuery(QueryHandle handle)
{
    VEX_ASSERT(queryPool);
    commandBuffer->writeTimestamp(::vk::PipelineStageFlagBits::eBottomOfPipe,
                                  queryPool->GetNativeQueryPool(),
                                  (handle.GetIndex() * 2) + 1);
}

void VkCommandList::ResolveTimestampQueries(u32 firstQuery, u32 queryCount)
{
    VEX_ASSERT(queryPool);
    commandBuffer->copyQueryPoolResults(queryPool->GetNativeQueryPool(),
                                        firstQuery,
                                        queryCount,
                                        queryPool->GetTimestampBuffer().GetNativeBuffer(),
                                        firstQuery * sizeof(u64),
                                        sizeof(u64),
                                        ::vk::QueryResultFlagBits::e64 | ::vk::QueryResultFlagBits::eWait);
}

void VkCommandList::BuildBLAS(RHIAccelerationStructure& as, RHIBuffer& scratchBuffer)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::BuildTLAS(RHIAccelerationStructure& as,
                              RHIBuffer& scratchBuffer,
                              RHIBuffer& uploadBuffer,
                              const RHITLASBuildDesc& desc)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkCommandList::Copy(RHITexture& src, RHITexture& dst, Span<const TextureCopyDesc> textureCopyDescriptions)
{
    const auto& srcDesc = src.desc;
    const auto& dstDesc = dst.desc;

    std::vector<::vk::ImageCopy> copyRegions{};

    const ::vk::ImageAspectFlags srcAspectMask = VkTextureUtil::GetFormatAspectFlags(srcDesc.format);
    const ::vk::ImageAspectFlags dstAspectMask = VkTextureUtil::GetFormatAspectFlags(dstDesc.format);

    copyRegions.reserve(textureCopyDescriptions.size());
    for (const auto& [bufferRegion, textureRegion] : textureCopyDescriptions)
    {
        copyRegions.push_back(::vk::ImageCopy{
            .srcSubresource = { .aspectMask = srcAspectMask,
                                .mipLevel = bufferRegion.subresource.startMip,
                                .baseArrayLayer = bufferRegion.subresource.startSlice,
                                .layerCount = bufferRegion.subresource.GetSliceCount(src.desc) },
            .srcOffset = ::vk::Offset3D(bufferRegion.offset.x, bufferRegion.offset.y, bufferRegion.offset.z),
            .dstSubresource = { .aspectMask = dstAspectMask,
                                .mipLevel = textureRegion.subresource.startMip,
                                .baseArrayLayer = textureRegion.subresource.startSlice,
                                .layerCount = textureRegion.subresource.GetSliceCount(dst.desc) },
            .dstOffset = ::vk::Offset3D(textureRegion.offset.x, textureRegion.offset.y, textureRegion.offset.z),
            .extent = ::vk::Extent3D{
                textureRegion.extent.GetWidth(dstDesc, textureRegion.subresource.startMip),
                textureRegion.extent.GetHeight(dstDesc, textureRegion.subresource.startMip),
                textureRegion.extent.GetDepth(dstDesc, textureRegion.subresource.startMip),
            } });
    }

    commandBuffer->copyImage(src.GetResource(),
                             ::vk::ImageLayout::eGeneral,
                             dst.GetResource(),
                             ::vk::ImageLayout::eGeneral,
                             static_cast<u32>(copyRegions.size()),
                             copyRegions.data());
}

void VkCommandList::Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDescription)
{
    const ::vk::BufferCopy copy{ .srcOffset = bufferCopyDescription.srcOffset,
                                 .dstOffset = bufferCopyDescription.dstOffset,
                                 .size = bufferCopyDescription.byteSize };
    commandBuffer->copyBuffer(src.GetNativeBuffer(), dst.GetNativeBuffer(), 1, &copy);
}

void VkCommandList::Copy(RHIBuffer& src, RHITexture& dst, Span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto regions = CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(dst, copyDescriptions);
    commandBuffer->copyBufferToImage(src.GetNativeBuffer(),
                                     dst.GetResource(),
                                     ::vk::ImageLayout::eGeneral,
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}

void VkCommandList::Copy(RHITexture& src, RHIBuffer& dst, Span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto regions = CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(src, copyDescriptions);
    commandBuffer->copyImageToBuffer(src.GetResource(),
                                     ::vk::ImageLayout::eGeneral,
                                     dst.GetNativeBuffer(),
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}

RHIScopedGPUEvent VkCommandList::CreateScopedMarker(const char* label, std::array<float, 3> labelColor)
{
    return { *this, label, labelColor };
}

VkCommandList::VkCommandList(NonNullPtr<VkGPUContext> ctx, ::vk::UniqueCommandBuffer&& commandBuffer, QueueType type)
    : RHICommandListBase{ type }
    , ctx{ ctx }
    , commandBuffer{ std::move(commandBuffer) }
{
}

} // namespace vex::vk
