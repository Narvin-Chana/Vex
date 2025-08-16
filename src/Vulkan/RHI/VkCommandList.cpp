#include "VkCommandList.h"

#include <Vex/Bindings.h>
#include <Vex/DrawHelpers.h>
#include <Vex/RHIBindings.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkGraphicsPipeline.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkErrorHandler.h>

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

    // VEX_VK_CHECK << commandBuffer->reset();

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

void VkCommandList::SetLayout(RHIResourceLayout& layout, RHIDescriptorPool& descriptorPool)
{
    bool hasGlobalConstantsBuffer = layout.currentInternalConstantBuffer.has_value();
    if (hasGlobalConstantsBuffer)
    {
        Transition(*layout.currentInternalConstantBuffer, RHIBufferState::UniformResource);
    }
    std::span<const u8> localConstantsData = layout.GetLocalConstantsData();

    size_t globalConstantsByteSize = sizeof(u32);
    size_t finalDataByteSize = localConstantsData.size_bytes() + globalConstantsByteSize;

    std::vector<u8> finalData(finalDataByteSize);
    std::memcpy(finalData.data() + globalConstantsByteSize, localConstantsData.data(), localConstantsData.size_bytes());
    if (hasGlobalConstantsBuffer)
    {
        u32 globalBindlessBuffer =
            layout.currentInternalConstantBuffer
                ->GetOrCreateBindlessView(BufferBindingUsage::ConstantBuffer, globalConstantsByteSize, descriptorPool)
                .GetIndex();
        std::memcpy(finalData.data(), reinterpret_cast<u8*>(&globalBindlessBuffer), globalConstantsByteSize);
    }
    else
    {
        u32 invalidBindlessHandle = GInvalidBindlessHandle.GetIndex();
        std::memcpy(finalData.data(), reinterpret_cast<u8*>(&invalidBindlessHandle), globalConstantsByteSize);
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

    commandBuffer->pushConstants(*layout.pipelineLayout, stageFlags, 0, finalData.size(), finalData.data());
}

void VkCommandList::SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout)
{
    switch (type)
    {
    case CommandQueueTypes::Graphics:
        commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eGraphics,
                                          *resourceLayout.pipelineLayout,
                                          0,
                                          1,
                                          &*descriptorPool.bindlessSet,
                                          0,
                                          nullptr);
    case CommandQueueTypes::Compute:
        commandBuffer->bindDescriptorSets(::vk::PipelineBindPoint::eCompute,
                                          *resourceLayout.pipelineLayout,
                                          0,
                                          1,
                                          &*descriptorPool.bindlessSet,
                                          0,
                                          nullptr);
        break;
    default:
        VEX_ASSERT(false, "Operation not supported on this queue type");
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
    const TextureDescription& desc = binding.texture->GetDescription();

    u32 layerCount = binding.binding.sliceCount == 0 ? desc.depthOrArraySize : binding.binding.sliceCount;
    u32 mipCount = binding.binding.mipCount == 0 ? desc.mips : binding.binding.mipCount;
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

    if (usage == TextureUsage::DepthStencil &&
        clearValue.flags & (TextureClear::ClearDepth | TextureClear::ClearStencil))
    {
        ::vk::ClearDepthStencilValue clearVal{
            .depth = clearValue.depth,
            .stencil = clearValue.stencil,
        };

        ::vk::ImageSubresourceRange ranges{
            .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
            .baseMipLevel = binding.binding.mipBias,
            .levelCount = mipCount,
            .baseArrayLayer = binding.binding.startSlice,
            .layerCount = layerCount,
        };

        commandBuffer->clearDepthStencilImage(binding.texture->GetResource(),
                                              binding.texture->GetLayout(),
                                              &clearVal,
                                              1,
                                              &ranges);
    }
    else
    {
        ::vk::ClearColorValue clearVal{ .float32 = clearValue.color };

        ::vk::ImageSubresourceRange ranges{
            .aspectMask = static_cast<::vk::ImageAspectFlagBits>(clearValue.flags),
            .baseMipLevel = binding.binding.mipBias,
            .levelCount = mipCount,
            .baseArrayLayer = binding.binding.startSlice,
            .layerCount = layerCount,
        };

        commandBuffer->clearColorImage(binding.texture->GetResource(),
                                       binding.texture->GetLayout(),
                                       &clearVal,
                                       1,
                                       &ranges);
    }
}

::vk::ImageMemoryBarrier2 GetMemoryBarrierFrom(VkTexture& texture, RHITextureState::Flags flags)
{
    using namespace ::vk;
    ImageLayout prevLayout = texture.GetLayout();
    ImageLayout nextLayout = TextureUtil::TextureStateFlagToImageLayout(flags);

    ImageMemoryBarrier2 barrier{
        .oldLayout = prevLayout,
        .newLayout = nextLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.GetResource(),
        .subresourceRange = {
            .aspectMask = ImageAspectFlagBits::eColor, // TODO(https://trello.com/c/whUAeix0): Support depth/stencil (will probably be fixed in merge w/ Vk graphics pipeline).
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    const bool isBackbufferImage = texture.IsBackBufferTexture();

    // === SOURCE STAGE AND ACCESS ===
    switch (prevLayout)
    {
    case ImageLayout::eUndefined:
        // Undefined layout means no previous access, so no source access mask needed
        barrier.srcAccessMask = AccessFlagBits2::eNone;
        // Swapchain images need to sync with acquire operation
        // Regular textures can start from top of pipe
        barrier.srcStageMask =
            isBackbufferImage ? PipelineStageFlagBits2::eColorAttachmentOutput : PipelineStageFlagBits2::eTopOfPipe;
        break;
    case ImageLayout::eColorAttachmentOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eColorAttachmentWrite | AccessFlagBits2::eColorAttachmentRead;
        barrier.srcStageMask = PipelineStageFlagBits2::eColorAttachmentOutput;
        break;
    case ImageLayout::eDepthStencilAttachmentOptimal:
        barrier.srcAccessMask =
            AccessFlagBits2::eDepthStencilAttachmentRead | AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.srcStageMask = PipelineStageFlagBits2::eEarlyFragmentTests | PipelineStageFlagBits2::eLateFragmentTests;
        break;
    case ImageLayout::eShaderReadOnlyOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eShaderRead;
        barrier.srcStageMask = PipelineStageFlagBits2::eVertexShader | PipelineStageFlagBits2::eFragmentShader |
                               PipelineStageFlagBits2::eComputeShader;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eTransferRead;
        barrier.srcStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eTransferDstOptimal:
        barrier.srcAccessMask = AccessFlagBits2::eTransferWrite;
        barrier.srcStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::ePresentSrcKHR:
        // Present layout has no specific access requirements
        barrier.srcAccessMask = AccessFlagBits2::eNone;
        barrier.srcStageMask =
            isBackbufferImage ? PipelineStageFlagBits2::eColorAttachmentOutput : PipelineStageFlagBits2::eAllCommands;
        break;
    case ImageLayout::eGeneral:
        // General layout allows any access
        barrier.srcAccessMask = AccessFlagBits2::eMemoryRead | AccessFlagBits2::eMemoryWrite;
        barrier.srcStageMask = PipelineStageFlagBits2::eAllCommands;
        break;
    default:
        VEX_ASSERT(false, "Unsupported source image layout: {}", to_string(prevLayout));
        break;
    }

    // === DESTINATION STAGE AND ACCESS ===
    switch (nextLayout)
    {
    case ImageLayout::eColorAttachmentOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eColorAttachmentWrite | AccessFlagBits2::eColorAttachmentRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eColorAttachmentOutput;
        break;
    case ImageLayout::eDepthStencilAttachmentOptimal:
        barrier.dstAccessMask =
            AccessFlagBits2::eDepthStencilAttachmentRead | AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eEarlyFragmentTests;
        break;
    case ImageLayout::eShaderReadOnlyOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eShaderRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eVertexShader | PipelineStageFlagBits2::eFragmentShader |
                               PipelineStageFlagBits2::eComputeShader;
        break;
    case ImageLayout::eTransferSrcOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eTransferRead;
        barrier.dstStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::eTransferDstOptimal:
        barrier.dstAccessMask = AccessFlagBits2::eTransferWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eTransfer;
        break;
    case ImageLayout::ePresentSrcKHR:
        // Present layout preparation
        barrier.dstAccessMask = AccessFlagBits2::eNone;
        // Swapchain images need to sync with present operation
        barrier.dstStageMask =
            isBackbufferImage ? PipelineStageFlagBits2::eColorAttachmentOutput : PipelineStageFlagBits2::eAllCommands;
        break;
    case ImageLayout::eGeneral:
        // General layout allows any access
        barrier.dstAccessMask = AccessFlagBits2::eMemoryRead | AccessFlagBits2::eMemoryWrite;
        barrier.dstStageMask = PipelineStageFlagBits2::eAllCommands;
        break;
    case ImageLayout::eUndefined:
        // Should never transition TO undefined
        VEX_ASSERT(false, "Cannot transition TO undefined layout");
        break;
    default:
        VEX_ASSERT(false, "Unsupported destination image layout: {}", to_string(nextLayout));
        break;
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

VkCommandList::VkCommandList(NonNullPtr<VkGPUContext> ctx,
                             ::vk::UniqueCommandBuffer&& commandBuffer,
                             CommandQueueType type)
    : ctx{ ctx }
    , commandBuffer{ std::move(commandBuffer) }
    , type{ type }
{
}

} // namespace vex::vk
