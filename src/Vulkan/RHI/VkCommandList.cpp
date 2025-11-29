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

    // Must have the same aspect mask
    if (a.subresourceRange.aspectMask != b.subresourceRange.aspectMask)
        return false;

    // Check if subresources are contiguous
    // Case 1: Adjacent mips in the same array layer
    if (a.subresourceRange.baseArrayLayer == b.subresourceRange.baseArrayLayer &&
        a.subresourceRange.layerCount == b.subresourceRange.layerCount && a.subresourceRange.layerCount == 1)
    {
        // Check if mips are adjacent
        u32 aLastMip = a.subresourceRange.baseMipLevel + a.subresourceRange.levelCount;
        if (aLastMip == b.subresourceRange.baseMipLevel)
            return true;
    }

    // Case 2: Adjacent array layers with the same mip range
    if (a.subresourceRange.baseMipLevel == b.subresourceRange.baseMipLevel &&
        a.subresourceRange.levelCount == b.subresourceRange.levelCount)
    {
        // Check if array layers are adjacent
        u32 aLastLayer = a.subresourceRange.baseArrayLayer + a.subresourceRange.layerCount;
        if (aLastLayer == b.subresourceRange.baseArrayLayer)
            return true;
    }

    return false;
}

static ::vk::ImageMemoryBarrier2 MergeBarriers(const ::vk::ImageMemoryBarrier2& a, const ::vk::ImageMemoryBarrier2& b)
{
    ::vk::ImageMemoryBarrier2 merged = a;

    // Merge adjacent mips (same array layer)
    if (a.subresourceRange.baseArrayLayer == b.subresourceRange.baseArrayLayer &&
        a.subresourceRange.layerCount == b.subresourceRange.layerCount)
    {
        merged.subresourceRange.baseMipLevel =
            std::min(a.subresourceRange.baseMipLevel, b.subresourceRange.baseMipLevel);
        merged.subresourceRange.levelCount = a.subresourceRange.levelCount + b.subresourceRange.levelCount;
    }
    // Merge adjacent array layers (same mip range)
    else if (a.subresourceRange.baseMipLevel == b.subresourceRange.baseMipLevel &&
             a.subresourceRange.levelCount == b.subresourceRange.levelCount)
    {
        merged.subresourceRange.baseArrayLayer =
            std::min(a.subresourceRange.baseArrayLayer, b.subresourceRange.baseArrayLayer);
        merged.subresourceRange.layerCount = a.subresourceRange.layerCount + b.subresourceRange.layerCount;
    }
    return merged;
}

static std::vector<::vk::BufferImageCopy> GetBufferImageCopyFromBufferToImageDescriptions(
    const RHITexture& texture, std::span<const BufferTextureCopyDesc> descriptions)
{
    const ::vk::ImageAspectFlags dstAspectMask =
        FormatUtil::IsDepthStencilCompatible(texture.GetDesc().format)
            ? ::vk::ImageAspectFlagBits::eDepth | ::vk::ImageAspectFlagBits::eStencil
            : ::vk::ImageAspectFlagBits::eColor;

    float pixelByteSize = TextureUtil::GetPixelByteSizeFromFormat(texture.GetDesc().format);

    std::vector<::vk::BufferImageCopy> regions;
    regions.reserve(descriptions.size());
    for (const auto& [bufferRegion, textureRegion] : descriptions)
    {
        u32 alignedRowPitch = static_cast<u32>(AlignUp<u64>(
            textureRegion.extent.GetWidth(texture.GetDesc(), textureRegion.subresource.startMip) * pixelByteSize,
            TextureUtil::RowPitchAlignment));

        regions.push_back(::vk::BufferImageCopy{
            .bufferOffset = bufferRegion.offset,
            // Buffer row length is in pixels.
            .bufferRowLength = static_cast<u32>(alignedRowPitch / pixelByteSize),
            .bufferImageHeight = 0,
            .imageSubresource =
                ::vk::ImageSubresourceLayers{
                    .aspectMask = dstAspectMask,
                    .mipLevel = textureRegion.subresource.startMip,
                    .baseArrayLayer = textureRegion.subresource.startSlice,
                    .layerCount = textureRegion.subresource.GetSliceCount(texture.GetDesc()),
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
    RHICommandListBase::Close();

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
                                 const TextureClearValue& clearValue)
{
    const ::vk::ImageSubresourceRange ranges{
        .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
        .baseMipLevel = binding.binding.subresource.startMip,
        .levelCount = binding.binding.subresource.GetMipCount(binding.texture->GetDesc()),
        .baseArrayLayer = binding.binding.subresource.startSlice,
        .layerCount = binding.binding.subresource.GetSliceCount(binding.texture->GetDesc()),
    };

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

            for (u16 mip = textureBarrier.subresource.startMip;
                 mip < textureBarrier.subresource.startMip + textureBarrier.subresource.GetMipCount(desc);
                 ++mip)
            {
                for (u32 slice = textureBarrier.subresource.startSlice;
                     slice < textureBarrier.subresource.startSlice +
                                 textureBarrier.subresource.GetSliceCount(textureBarrier.texture->GetDesc());
                     ++slice)
                {
                    ::vk::ImageMemoryBarrier2 vkBarrier = {};
                    if (textureBarrier.texture->IsBackBufferTexture() &&
                        textureBarrier.texture->GetLastLayoutForSubresource(mip, slice) == RHITextureLayout::Undefined)
                    {
                        // Synchronize with vkAcquireNextImageKHR
                        vkBarrier.srcStageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                        vkBarrier.srcAccessMask = ::vk::AccessFlagBits2::eNone;
                    }
                    else
                    {
                        vkBarrier.srcStageMask =
                            RHIBarrierSyncToVulkan(textureBarrier.texture->GetLastSyncForSubresource(mip, slice));
                        vkBarrier.srcAccessMask =
                            RHIBarrierAccessToVulkan(textureBarrier.texture->GetLastAccessForSubresource(mip, slice));
                    }
                    vkBarrier.oldLayout =
                        RHITextureLayoutToVulkan(textureBarrier.texture->GetLastLayoutForSubresource(mip, slice));
                    vkBarrier.dstStageMask = RHIBarrierSyncToVulkan(textureBarrier.dstSync);
                    vkBarrier.dstAccessMask = RHIBarrierAccessToVulkan(textureBarrier.dstAccess);
                    vkBarrier.newLayout = RHITextureLayoutToVulkan(textureBarrier.dstLayout);
                    vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    vkBarrier.image = textureBarrier.texture->GetResource();
                    vkBarrier.subresourceRange = { .aspectMask = VkTextureUtil::GetFormatAspectFlags(desc.format),
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
                                                                                  slice);
                    }
                }
            }

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

    std::vector<::vk::RenderingAttachmentInfo> colorAttachmentsInfo(resources.renderTargets.size());

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
        .pStencilAttachment =
            depthInfo && FormatUtil::SupportsStencil(resources.depthStencil->texture->GetDesc().format) ? &*depthInfo
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

void VkCommandList::GenerateMips(RHITexture& texture, const TextureSubresource& subresource)
{
    bool isDepthStencilFormat = FormatUtil::IsDepthStencilCompatible(texture.GetDesc().format);
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

void VkCommandList::Copy(RHITexture& src, RHITexture& dst, std::span<const TextureCopyDesc> textureCopyDescriptions)
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

void VkCommandList::Copy(RHIBuffer& src, RHITexture& dst, std::span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto regions = CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(dst, copyDescriptions);

    commandBuffer->copyBufferToImage(src.GetNativeBuffer(),
                                     dst.GetResource(),
                                     RHITextureLayoutToVulkan(dst.GetLastLayout()),
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}

void VkCommandList::Copy(RHITexture& src, RHIBuffer& dst, std::span<const BufferTextureCopyDesc> copyDescriptions)
{
    auto regions = CommandList_Internal::GetBufferImageCopyFromBufferToImageDescriptions(src, copyDescriptions);

    commandBuffer->copyImageToBuffer(src.GetResource(),
                                     RHITextureLayoutToVulkan(src.GetLastLayout()),
                                     dst.GetNativeBuffer(),
                                     static_cast<u32>(regions.size()),
                                     regions.data());
}
RHIScopedGPUEvent VkCommandList::CreateScopedMarker(const char* label, std::array<float, 3> labelColor)
{
    return { *commandBuffer, label, labelColor };
}

VkCommandList::VkCommandList(NonNullPtr<VkGPUContext> ctx, ::vk::UniqueCommandBuffer&& commandBuffer, QueueType type)
    : RHICommandListBase{ type }
    , ctx{ ctx }
    , commandBuffer{ std::move(commandBuffer) }
{
}

} // namespace vex::vk
