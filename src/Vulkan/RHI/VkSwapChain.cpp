#include "VkSwapChain.h"

#include <algorithm>
#include <utility>

#include <Vex/PlatformWindow.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIFence.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Synchronization.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

static VkSwapChainSupportDetails GetSwapChainSupportDetails(::vk::PhysicalDevice device, ::vk::SurfaceKHR surface)
{
    auto surfaceFormats = VEX_VK_CHECK <<= device.getSurfaceFormatsKHR(surface);
    auto surfacePresentModes = VEX_VK_CHECK <<= device.getSurfacePresentModesKHR(surface);
    auto surfaceCapabilities = VEX_VK_CHECK <<= device.getSurfaceCapabilitiesKHR(surface);

    return VkSwapChainSupportDetails{ surfaceCapabilities, surfaceFormats, surfacePresentModes };
}

static bool IsSwapChainSupported(::vk::PhysicalDevice device, ::vk::SurfaceKHR surface)
{
    return GetSwapChainSupportDetails(device, surface).IsValid();
}

static ::vk::SurfaceFormatKHR GetBestSurfaceFormat(const VkSwapChainSupportDetails& details,
                                                   ::vk::Format requestedFormat)
{
    for (const ::vk::SurfaceFormatKHR& availableFormat : details.formats)
    {
        if (availableFormat.format == requestedFormat &&
            availableFormat.colorSpace == ::vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    VEX_LOG(Fatal, "Format \"{}\" not supported", ::vk::to_string(requestedFormat));
    return {};
}

// Looks for mailbox which allows to always take the most recent image. Falls back to FIFO. If VSync is off, we instead
// take immediate mode.
static ::vk::PresentModeKHR GetBestPresentMode(const VkSwapChainSupportDetails& details, bool useVSync)
{
    if (useVSync)
        return ::vk::PresentModeKHR::eImmediate;

    // Included in <utility>, which is more light-weight versus <algorithm>/<ranges>.
    auto it = std::ranges::find(details.presentModes, ::vk::PresentModeKHR::eMailbox);
    return it != details.presentModes.end() ? *it : ::vk::PresentModeKHR::eFifo;
}

static ::vk::Extent2D GetBestSwapExtent(const VkSwapChainSupportDetails& details, u32 width, u32 height)
{
    if (details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return details.capabilities.currentExtent;

    ::vk::Extent2D actualExtent = { width, height };

    actualExtent.width = std::clamp(actualExtent.width,
                                    details.capabilities.minImageExtent.width,
                                    details.capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     details.capabilities.minImageExtent.height,
                                     details.capabilities.maxImageExtent.height);

    return actualExtent;
}

VkSwapChain::VkSwapChain(NonNullPtr<VkGPUContext> ctx,
                         const SwapChainDescription& desc,
                         const PlatformWindow& platformWindow)
    : ctx{ ctx }
    , desc{ desc }
{
    VEX_ASSERT(IsSwapChainSupported(ctx->physDevice, ctx->surface));

    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);
    surfaceFormat = GetBestSurfaceFormat(supportDetails, TextureFormatToVulkan(desc.format, false));
    presentMode = GetBestPresentMode(supportDetails, desc.useVSync);

    u32 maxSupportedImageCount =
        std::max(supportDetails.capabilities.minImageCount + 1, supportDetails.capabilities.maxImageCount);
    u8 requestedImageCount = std::to_underlying(desc.frameBuffering);

    // Need to have at least the requested amount of swap chain images
    VEX_ASSERT(maxSupportedImageCount >= requestedImageCount);

    InitSwapchainResource(platformWindow.width, platformWindow.height);

    auto BinarySemaphoreCreator = [&ctx]
    {
        ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eBinary };
        return VEX_VK_CHECK <<= ctx->device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
                   .pNext = &createInfo,
               });
    };
    // Requires including the heavy <algorithm>
    std::ranges::generate_n(std::back_inserter(backbufferAcquisition), requestedImageCount, BinarySemaphoreCreator);
    std::ranges::generate_n(std::back_inserter(presentSemaphore), requestedImageCount, BinarySemaphoreCreator);
}

void VkSwapChain::Resize(u32 width, u32 height)
{
    swapchain.reset();
    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);
    InitSwapchainResource(width, height);
}

TextureDesc VkSwapChain::GetBackBufferTextureDescription() const
{
    return TextureDesc{ .name = "backbuffer",
                               .type = TextureType::Texture2D,
                               .format = VulkanToTextureFormat(surfaceFormat.format),
                               .width = width,
                               .height = height,
                               .depthOrSliceCount = 1,
                               .mips = 1,
                               .usage = TextureUsage::RenderTarget | TextureUsage::ShaderRead };
}

void VkSwapChain::SetVSync(bool enableVSync)
{
    presentMode = GetBestPresentMode(supportDetails, enableVSync);
    desc.useVSync = enableVSync;
    InitSwapchainResource(width, height);
}

bool VkSwapChain::NeedsFlushForVSyncToggle()
{
    return true;
}

std::optional<RHITexture> VkSwapChain::AcquireBackBuffer(u8 frameIndex)
{
    // Acquire the next backbuffer image.
    auto res = ctx->device.acquireNextImageKHR(*swapchain,
                                               std::numeric_limits<u64>::max(),
                                               *backbufferAcquisition[frameIndex],
                                               VK_NULL_HANDLE,
                                               &currentBackbufferId);
    if (res == ::vk::Result::eErrorOutOfDateKHR)
    {
        return {};
    }

    VEX_VK_CHECK << res;

    // Return the acquired backbuffer.
    auto backbufferImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);

    TextureDesc desc = GetBackBufferTextureDescription();
    desc.name = std::format("backbuffer_{}", currentBackbufferId);
    return RHITexture{ ctx, std::move(desc), backbufferImages[currentBackbufferId] };
}

SyncToken VkSwapChain::Present(u8 frameIndex, RHI& rhi, NonNullPtr<RHICommandList> commandList, bool isFullscreen)
{
    ::vk::CommandBufferSubmitInfo cmdBufferSubmitInfo{ .commandBuffer = commandList->GetNativeCommandList() };

    // Before rendering the graphics queue, we must wait for acquireImage to be done.
    // This equates to waiting on the backbufferAcquisition binary semaphore of this backbuffer.
    ::vk::SemaphoreSubmitInfo acquireWaitInfo = {
        .semaphore = *backbufferAcquisition[frameIndex],
        .stageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };

    // Signal the present binary semaphore (we only want to present once rendering work is done).
    ::vk::SemaphoreSubmitInfo presentSignalInfo = {
        .semaphore = *presentSemaphore[frameIndex],
        .stageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };

    SyncToken syncToken = rhi.SubmitToQueue(commandList->GetType(),
                                            { &cmdBufferSubmitInfo, 1 },
                                            { &acquireWaitInfo, 1 },
                                            { presentSignalInfo });

    // Present now that we've submitted the present copy work.
    ::vk::PresentInfoKHR presentInfo = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*presentSemaphore[frameIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swapchain,
        .pImageIndices = &currentBackbufferId,
    };

    auto res = ctx->graphicsPresentQueue.queue.presentKHR(presentInfo);
    if (res != ::vk::Result::eErrorOutOfDateKHR)
    {
        VEX_VK_CHECK << res;
    }

    return syncToken;
}

void VkSwapChain::InitSwapchainResource(u32 inWidth, u32 inHeight)
{
    width = inWidth;
    height = inHeight;
    ::vk::Extent2D extent = GetBestSwapExtent(supportDetails, width, height);

    ::vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface = ctx->surface,
                                                      .minImageCount = std::to_underlying(desc.frameBuffering),
                                                      .imageFormat = surfaceFormat.format,
                                                      .imageColorSpace = surfaceFormat.colorSpace,
                                                      .imageExtent = extent,
                                                      .imageArrayLayers = 1,
                                                      .imageUsage = ::vk::ImageUsageFlagBits::eColorAttachment |
                                                                    ::vk::ImageUsageFlagBits::eTransferDst |
                                                                    ::vk::ImageUsageFlagBits::eTransferSrc,
                                                      .imageSharingMode = ::vk::SharingMode::eExclusive,
                                                      .queueFamilyIndexCount = 0,
                                                      .pQueueFamilyIndices = nullptr,
                                                      .preTransform = supportDetails.capabilities.currentTransform,
                                                      .compositeAlpha = ::vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                      .presentMode = presentMode,
                                                      .clipped = ::vk::True,
                                                      .oldSwapchain = {} };

    swapchain = VEX_VK_CHECK <<= ctx->device.createSwapchainKHRUnique(swapChainCreateInfo);

    auto newImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);
    if (newImages.size() != swapChainCreateInfo.minImageCount)
    {
        VEX_LOG(Warning, "Swapchain returned more images than requested for. This might cause instabilities");
    }
}

} // namespace vex::vk