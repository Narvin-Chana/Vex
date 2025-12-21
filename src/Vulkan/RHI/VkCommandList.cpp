#include "VkCommandList.h"

#include <algorithm>
#include <cmath>

#include <Vex/Bindings.h>
#include <Vex/DrawHelpers.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Utility/Algorithms.h>
#include <Vex/Utility/ByteUtils.h>

#include <RHI/RHIBindings.h>

#include <Vulkan/RHI/VkBarrier.h>
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkScopedGPUEvent.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/RHI/VkTimestampQueryPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkGraphicsPipeline.h>

namespace vex::vk
{

namespace CommandList_Internal
{

static bool CanMergeBarriers(const ::vk::ImageMemoryBarrier2& a, const ::vk::ImageMemoryBarrier2& b)
{
    // Must be for the same image
    if (a.image != b.image)
        return false;

    // Must have the same sync/access/layout transitions
    if (a.srcStageMask != b.srcStageMask || a.dstStageMask != b.dstStageMask)
        return false;
    if (a.srcAccessMask != b.srcAccessMask || a.dstAccessMask != b.dstAccessMask)
        return false;
    if (a.oldLayout != b.oldLayout || a.newLayout != b.newLayout)
        return false;

    // Must have the same queue family indices
    if (a.srcQueueFamilyIndex != b.srcQueueFamilyIndex || a.dstQueueFamilyIndex != b.dstQueueFamilyIndex)
        return false;

    // Must share an aspect mask bit
    if (!(a.subresourceRange.aspectMask & b.subresourceRange.aspectMask))
        return false;

    // Check if subresources are contiguous
    // Case 1: Adjacent mips in the same array layer
    if (a.subresourceRange.baseArrayLayer == b.subresourceRange.baseArrayLayer &&
        a.subresourceRange.layerCount == b.subresourceRange.layerCount)
    {
        // Check if mips are adjacent
        if (DoesRangeOverlap(a.subresourceRange.baseMipLevel,
                             a.subresourceRange.levelCount,
                             b.subresourceRange.baseMipLevel,
                             b.subresourceRange.levelCount))
            return true;
    }

    // Case 2: Adjacent array layers with the same mip range
    if (a.subresourceRange.baseMipLevel == b.subresourceRange.baseMipLevel &&
        a.subresourceRange.levelCount == b.subresourceRange.levelCount)
    {
        // Check if array layers are adjacent
        if (DoesRangeOverlap(a.subresourceRange.baseArrayLayer,
                             a.subresourceRange.layerCount,
                             b.subresourceRange.baseArrayLayer,
                             b.subresourceRange.layerCount))
            return true;
    }

    return false;
}

static ::vk::ImageMemoryBarrier2 MergeBarriers(const ::vk::ImageMemoryBarrier2& a, const ::vk::ImageMemoryBarrier2& b)
{
    ::vk::ImageMemoryBarrier2 merged = a;

    if (a.subresourceRange.baseArrayLayer == b.subresourceRange.baseArrayLayer &&
        a.subresourceRange.layerCount == b.subresourceRange.layerCount)
    {
        u32 firstMip = std::min(merged.subresourceRange.baseMipLevel, b.subresourceRange.baseMipLevel);
        u32 lastMip = std::max(merged.subresourceRange.baseMipLevel + merged.subresourceRange.levelCount,
                               b.subresourceRange.baseMipLevel + b.subresourceRange.levelCount);
        merged.subresourceRange.baseMipLevel = firstMip;
        merged.subresourceRange.levelCount = lastMip - firstMip;
    }
    else if (merged.subresourceRange.baseMipLevel == b.subresourceRange.baseMipLevel &&
             merged.subresourceRange.levelCount == b.subresourceRange.levelCount)
    {
        u32 firstSlice = std::min(merged.subresourceRange.baseArrayLayer, b.subresourceRange.baseArrayLayer);
        u32 lastSlice = std::max(merged.subresourceRange.baseArrayLayer + merged.subresourceRange.layerCount,
                                 b.subresourceRange.baseArrayLayer + b.subresourceRange.layerCount);
        merged.subresourceRange.baseArrayLayer = firstSlice;
        merged.subresourceRange.layerCount = lastSlice - firstSlice;
    }

    merged.subresourceRange.aspectMask |= b.subresourceRange.aspectMask;

    return merged;
}

static std::pair<::vk::ImageLayout, std::vector<::vk::BufferImageCopy>> GetBufferImageCopyFromBufferToImageDescriptions(
    const RHITexture& texture, Span<const BufferTextureCopyDesc> descriptions)
{
    std::optional<RHITextureLayout> layout{};

    std::vector<::vk::BufferImageCopy> regions;
    regions.reserve(descriptions.size());
    for (const auto& [bufferRegion, textureRegion] : descriptions)
    {
        float pixelByteSize = TextureUtil::GetPixelByteSizeFromFormat(
            TextureUtil::GetCopyFormat(texture.GetDesc().format, textureRegion.subresource.GetSingleAspect()));

        u32 alignedRowPitch = static_cast<u32>(AlignUp<u64>(
            textureRegion.extent.GetWidth(texture.GetDesc(), textureRegion.subresource.startMip) * pixelByteSize,
            TextureUtil::RowPitchAlignment));

        TextureAspect::Type aspect = textureRegion.subresource.GetSingleAspect();

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

        for (u32 i = startLayer; i < startLayer + layerCount; ++i)
        {
            auto subresourceLayout =
                texture.GetLastLayoutForSubresource(textureRegion.subresource.startMip,
                                                    i,
                                                    TextureUtil::TextureAspectToPlaneIndex(aspect));
            if (!layout)
            {
                layout = subresourceLayout;
            }
            else
            {
                VEX_CHECK(*layout == subresourceLayout,
                          "All regions must have the same subresource layout when attempting a image copy");
            }
        }
    }

    return { RHITextureLayoutToVulkan(*layout), regions };
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
                                 std::span<TextureClearRect> clearRects)
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
        RHITextureBarrier textureBarrier{ binding.texture,
                                          { .aspect = aspect },
                                          RHIBarrierSync::Clear,
                                          RHIBarrierAccess::CopyDest,
                                          RHITextureLayout::CopyDest };
        Barrier({}, { &textureBarrier, 1 });

        if (usage == TextureUsage::DepthStencil &&
            clearValue.clearAspect & (TextureAspect::Depth | TextureAspect::Stencil))
        {
            ::vk::ClearDepthStencilValue clearVal{
                .depth = clearValue.depth,
                .stencil = clearValue.stencil,
            };
            commandBuffer->clearDepthStencilImage(
                binding.texture->GetResource(),
                RHITextureLayoutToVulkan(binding.texture->GetLastLayoutForSubresource(binding.binding.subresource)),
                &clearVal,
                1,
                &ranges);
        }
        else
        {
            ::vk::ClearColorValue clearVal{ .float32 = clearValue.color };
            commandBuffer->clearColorImage(
                binding.texture->GetResource(),
                RHITextureLayoutToVulkan(binding.texture->GetLastLayoutForSubresource(binding.binding.subresource)),
                &clearVal,
                1,
                &ranges);
        }
    }
    else
    {
        RHIDrawResources resources;
        std::optional<RHITextureBarrier> textureBarrier;

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
            textureBarrier.emplace(binding.texture,
                                   TextureSubresource{ .aspect = aspect },
                                   RHIBarrierSync::DepthStencil,
                                   RHIBarrierAccess::DepthStencilRead,
                                   RHITextureLayout::DepthStencilRead);
        }
        else
        {
            resources.renderTargets.push_back(binding);
            clearAttachment.clearValue.color = ::vk::ClearColorValue{ .float32 = clearValue.color };
            textureBarrier.emplace(binding.texture,
                                   TextureSubresource{ .aspect = aspect },
                                   RHIBarrierSync::RenderTarget,
                                   RHIBarrierAccess::RenderTarget,
                                   RHITextureLayout::RenderTarget);
        }

        Barrier({}, { &*textureBarrier, 1 });
        BeginRendering(resources);
        commandBuffer->clearAttachments(1, &clearAttachment, rects.size(), rects.data());
        EndRendering();
    }
}

void VkCommandList::Barrier(Span<const RHIBufferBarrier> bufferBarriers, Span<const RHITextureBarrier> textureBarriers)
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
        // Check if we can use the fast-path.
        const bool isSubresourceFullResource = textureBarrier.subresource == TextureSubresource{};
        if (isSubresourceFullResource && textureBarrier.texture->IsLastBarrierStateUniform())
        {
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

            // Update last barrier state for the resource.
            textureBarrier.texture->SetLastBarrierState(textureBarrier.dstSync,
                                                        textureBarrier.dstAccess,
                                                        textureBarrier.dstLayout);
        }
        else
        {
            // Ensures the texture uses non-uniform last barrier states.
            textureBarrier.texture->EnsureLastBarrierStateNonUniform();

            TextureUtil::ForEachSubresourceIndices(
                textureBarrier.subresource,
                textureBarrier.texture->GetDesc(),
                [&](u32 mip, u32 slice, u32 plane)
                {
                    ::vk::ImageMemoryBarrier2 vkBarrier = {};
                    if (textureBarrier.texture->IsBackBufferTexture() &&
                        textureBarrier.texture->GetLastLayoutForSubresource(mip, slice, plane) ==
                            RHITextureLayout::Undefined)
                    {
                        // Synchronize with vkAcquireNextImageKHR
                        vkBarrier.srcStageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                        vkBarrier.srcAccessMask = ::vk::AccessFlagBits2::eNone;
                    }
                    else
                    {
                        vkBarrier.srcStageMask = RHIBarrierSyncToVulkan(
                            textureBarrier.texture->GetLastSyncForSubresource(mip, slice, plane));
                        vkBarrier.srcAccessMask = RHIBarrierAccessToVulkan(
                            textureBarrier.texture->GetLastAccessForSubresource(mip, slice, plane));
                    }
                    vkBarrier.oldLayout = RHITextureLayoutToVulkan(
                        textureBarrier.texture->GetLastLayoutForSubresource(mip, slice, plane));
                    vkBarrier.dstStageMask = RHIBarrierSyncToVulkan(textureBarrier.dstSync);
                    vkBarrier.dstAccessMask = RHIBarrierAccessToVulkan(textureBarrier.dstAccess);
                    vkBarrier.newLayout = RHITextureLayoutToVulkan(textureBarrier.dstLayout);
                    vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    vkBarrier.image = textureBarrier.texture->GetResource();
                    vkBarrier.subresourceRange = { .aspectMask = VkTextureUtil::AspectFlagFromPlaneIndex(
                                                       textureBarrier.texture->GetDesc().format,
                                                       plane),
                                                   .baseMipLevel = mip,
                                                   .levelCount = 1,
                                                   .baseArrayLayer = slice,
                                                   .layerCount = 1 },
                    vkTextureBarriers.push_back(std::move(vkBarrier));

                    if (!isSubresourceFullResource)
                    {
                        // Update last barrier state for the subresource.
                        textureBarrier.texture->SetLastBarrierStateForSubresource(textureBarrier.dstSync,
                                                                                  textureBarrier.dstAccess,
                                                                                  textureBarrier.dstLayout,
                                                                                  mip,
                                                                                  slice,
                                                                                  plane);
                    }
                });

            // If the dst barrier is constant across the entire resource, we can just revert to uniform barriers.
            if (isSubresourceFullResource)
            {
                textureBarrier.texture->SetLastBarrierState(textureBarrier.dstSync,
                                                            textureBarrier.dstAccess,
                                                            textureBarrier.dstLayout);
            }
        }
    }

    std::vector<::vk::ImageMemoryBarrier2> compactedVkTextureBarriers;
    compactedVkTextureBarriers.reserve(vkTextureBarriers.size());
    for (u32 i = 0; i < vkTextureBarriers.size(); ++i)
    {
        ::vk::ImageMemoryBarrier2 current = vkTextureBarriers[i];
        // Keep merging while possible.
        while (i + 1 < vkTextureBarriers.size() &&
               CommandList_Internal::CanMergeBarriers(current, vkTextureBarriers[i + 1]))
        {
            current = CommandList_Internal::MergeBarriers(current, vkTextureBarriers[i + 1]);
            ++i; // Skip the merged barrier
        }
        compactedVkTextureBarriers.push_back(current);
    }

    VEX_CHECK(!vkBufferBarriers.empty() || !vkTextureBarriers.empty(),
              "TextureBarriers and BufferBarriers cannot both be empty...");
    ::vk::DependencyInfo info{ .bufferMemoryBarrierCount = static_cast<u32>(vkBufferBarriers.size()),
                               .pBufferMemoryBarriers = vkBufferBarriers.data(),
                               .imageMemoryBarrierCount = static_cast<u32>(compactedVkTextureBarriers.size()),
                               .pImageMemoryBarriers = compactedVkTextureBarriers.data() };
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
                .imageLayout = RHITextureLayoutToVulkan(
                    rtBindings.texture->GetLastLayoutForSubresource(rtBindings.binding.subresource)),
            };
        });

    std::optional<::vk::RenderingAttachmentInfo> depthInfo;
    if (resources.depthStencil && resources.depthStencil->texture->GetDesc().usage & TextureUsage::DepthStencil)
    {
        depthInfo = ::vk::RenderingAttachmentInfo{
            .imageView = resources.depthStencil->texture->GetOrCreateImageView(resources.depthStencil->binding,
                                                                               TextureUsage::DepthStencil),
            .imageLayout = RHITextureLayoutToVulkan(resources.depthStencil->texture->GetLastLayoutForSubresource(
                resources.depthStencil->binding.subresource)),
        };
    };

    const ::vk::RenderingInfo info{
        .renderArea = maxArea,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentsInfo.size()),
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

void VkCommandList::TraceRays(const std::array<u32, 3>& widthHeightDepth,
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

    // Transition all mips before the sourceMip (inclusive) to transferDst.
    RHITextureBarrier barrier{ texture,
                               TextureSubresource{
                                   .startMip = 0,
                                   .mipCount = static_cast<u16>(sourceMip + 1),
                                   .startSlice = subresource.startSlice,
                                   .sliceCount = subresource.sliceCount,
                               },
                               RHIBarrierSync::Blit,
                               RHIBarrierAccess::CopyDest,
                               RHITextureLayout::CopyDest };
    Barrier({}, { &barrier, 1 });

    i32 mipWidth = std::max<i32>(1, texture.GetDesc().width >> sourceMip);
    i32 mipHeight = std::max<i32>(1, texture.GetDesc().height >> sourceMip);
    i32 mipDepth = std::max<i32>(1, texture.GetDesc().GetDepth() >> sourceMip);

    for (u16 i = sourceMip + 1; i < subresource.startMip + subresource.GetMipCount(texture.desc); ++i)
    {
        // Transition the (i-1)th mip to transferSrc.
        barrier.subresource.startMip = i - 1;
        barrier.subresource.mipCount = 1;
        barrier.dstSync = RHIBarrierSync::Copy;
        barrier.dstAccess = RHIBarrierAccess::CopySource;
        barrier.dstLayout = RHITextureLayout::CopySource;
        Barrier({}, { &barrier, 1 });

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
    barrier.subresource.startMip = subresource.startMip + subresource.GetMipCount(texture.desc) - 1;
    barrier.subresource.mipCount = 1;
    barrier.dstSync = RHIBarrierSync::Copy;
    barrier.dstAccess = RHIBarrierAccess::CopySource;
    barrier.dstLayout = RHITextureLayout::CopySource;
    Barrier({}, { &barrier, 1 });
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
                                 .size = bufferCopyDescription.byteSize };
    commandBuffer->copyBuffer(src.GetNativeBuffer(), dst.GetNativeBuffer(), 1, &copy);
}

void VkCommandList::Copy(RHIBuffer& src, RHITexture& dst, Span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto [layout, regions] =
        CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(dst, copyDescriptions);

    commandBuffer->copyBufferToImage(src.GetNativeBuffer(),
                                     dst.GetResource(),
                                     layout,
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}

void VkCommandList::Copy(RHITexture& src, RHIBuffer& dst, Span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto [layout, regions] =
        CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(src, copyDescriptions);

    commandBuffer->copyImageToBuffer(src.GetResource(),
                                     layout,
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
