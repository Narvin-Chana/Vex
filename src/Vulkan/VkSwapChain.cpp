#include "VkSwapChain.h"

#include <Vex/PlatformWindow.h>

#include "VkCommandQueue.h"
#include "VkErrorHandler.h"
#include "VkFormats.h"
#include "VkGPUContext.h"
#include "VkHeaders.h"

#include "VkTexture.h"

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

// currently looks for mailbox which allows to always take the most recent image
// fallsback on FIFO
// if VSync is off, take immediate mode
static ::vk::PresentModeKHR GetBestPresentMode(const VkSwapChainSupportDetails& details, bool useVSync)
{
    if (useVSync)
        return ::vk::PresentModeKHR::eImmediate;

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

VkSwapChain::VkSwapChain(VkGPUContext& ctx,
                         const SwapChainDescription& description,
                         const PlatformWindow& platformWindow)
    : description{ description }
    , ctx{ ctx }
{
    VEX_ASSERT(IsSwapChainSupported(ctx.physDevice, ctx.surface));

    supportDetails = GetSwapChainSupportDetails(ctx.physDevice, ctx.surface);
    surfaceFormat = GetBestSurfaceFormat(supportDetails, TextureFormatToVulkan(description.format));
    presentMode = GetBestPresentMode(supportDetails, description.useVSync);

    u32 maxSupportedImageCount =
        std::max(supportDetails.capabilities.minImageCount + 1, supportDetails.capabilities.maxImageCount);
    u8 requestedImageCount = std::to_underlying(description.frameBuffering);

    // Need to have at least the requested amount of swap chain images
    VEX_ASSERT(maxSupportedImageCount >= requestedImageCount);

    InitSwapchainResource(platformWindow.width, platformWindow.height);

    std::ranges::generate_n(std::back_inserter(backbufferAcquisition),
                            requestedImageCount,
                            [&] { return VEX_VK_CHECK <<= ctx.device.createSemaphoreUnique({}); });
}

void VkSwapChain::AcquireNextBackbuffer(u8 frameIndex)
{
    VEX_VK_CHECK << ctx.device.acquireNextImageKHR(*swapchain,
                                                   std::numeric_limits<u64>::max(),
                                                   *backbufferAcquisition[frameIndex],
                                                   nullptr,
                                                   &currentBackbufferId);

    auto& cmdQueue = ctx.graphicsPresentQueue;

    ::vk::SemaphoreSubmitInfo semWaitInfo{
        .semaphore = *backbufferAcquisition[frameIndex],
    };

    ::vk::SemaphoreSubmitInfo semSignalInfo{
        .semaphore = *cmdQueue.waitSemaphore,
        .value = cmdQueue.waitValue + 1,
    };

    ::vk::SubmitInfo2KHR submitInfo{ .waitSemaphoreInfoCount = 1,
                                     .pWaitSemaphoreInfos = &semWaitInfo,
                                     .signalSemaphoreInfoCount = 1,
                                     .pSignalSemaphoreInfos = &semSignalInfo };

    VEX_VK_CHECK << cmdQueue.queue.submit2KHR(submitInfo);
}

void VkSwapChain::Present()
{
    auto& cmdQueue = ctx.graphicsPresentQueue;

    ::vk::SemaphoreSubmitInfo semWaitInfo{
        .semaphore = *cmdQueue.waitSemaphore,
        .value = cmdQueue.waitValue,
    };

    ::vk::SemaphoreSubmitInfo semSignalInfo{
        .semaphore = *presentSemaphore[currentBackbufferId],
    };

    ::vk::SubmitInfo2KHR submitInfo{ .waitSemaphoreInfoCount = 1,
                                     .pWaitSemaphoreInfos = &semWaitInfo,
                                     .signalSemaphoreInfoCount = 1,
                                     .pSignalSemaphoreInfos = &semSignalInfo };

    VEX_VK_CHECK << cmdQueue.queue.submit2KHR(submitInfo);

    ::vk::SwapchainKHR swapChains[] = { *swapchain };

    ::vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*presentSemaphore[currentBackbufferId],
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &currentBackbufferId,
    };

    VEX_VK_CHECK << cmdQueue.queue.presentKHR(presentInfo);
}

void VkSwapChain::Resize(u32 width, u32 height)
{
    InitSwapchainResource(width, height);
}

void VkSwapChain::SetVSync(bool enableVSync)
{
    presentMode = GetBestPresentMode(supportDetails, enableVSync);
    description.useVSync = enableVSync;
    InitSwapchainResource(width, height);
}

bool VkSwapChain::NeedsFlushForVSyncToggle()
{
    return true;
}

UniqueHandle<RHITexture> VkSwapChain::CreateBackBuffer(u8 backBufferIndex)
{
    auto backbufferImages = VEX_VK_CHECK <<= ctx.device.getSwapchainImagesKHR(*swapchain);

    TextureDescription desc{
        .name = std::format("backbuffer_{}", backBufferIndex),
        .type = TextureType::Texture2D,
        .width = width,
        .height = height,
        .depthOrArraySize = 1,
        .mips = 1,
        .format = VulkanToTextureFormat(surfaceFormat.format),
    };

    return MakeUnique<VkBackbufferTexture>(std::move(desc), backbufferImages[backBufferIndex]);
}

void VkSwapChain::InitSwapchainResource(u32 inWidth, u32 inHeight)
{
    width = inWidth;
    height = inHeight;
    ::vk::Extent2D extent = GetBestSwapExtent(supportDetails, width, height);

    ::vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface = ctx.surface,
                                                      .minImageCount = std::to_underlying(description.frameBuffering),
                                                      .imageFormat = surfaceFormat.format,
                                                      .imageColorSpace = surfaceFormat.colorSpace,
                                                      .imageExtent = extent,
                                                      .imageArrayLayers = 1,
                                                      .imageUsage = ::vk::ImageUsageFlagBits::eColorAttachment,
                                                      .imageSharingMode = ::vk::SharingMode::eExclusive,
                                                      .queueFamilyIndexCount = 0,
                                                      .pQueueFamilyIndices = nullptr,
                                                      .preTransform = supportDetails.capabilities.currentTransform,
                                                      .compositeAlpha = ::vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                      .presentMode = presentMode,
                                                      .clipped = ::vk::True,
                                                      .oldSwapchain = {} };

    swapchain = VEX_VK_CHECK <<= ctx.device.createSwapchainKHRUnique(swapChainCreateInfo);

    auto newImages = VEX_VK_CHECK <<= ctx.device.getSwapchainImagesKHR(*swapchain);
    if (newImages.size() != swapChainCreateInfo.minImageCount)
    {
        VEX_LOG(Warning, "Swapchain returned more images than requested for. This might cause instabilities");
    }

    presentSemaphore.resize(newImages.size());
    for (size_t i = 0; i < newImages.size(); ++i)
    {
        presentSemaphore[i] = VEX_VK_CHECK <<= ctx.device.createSemaphoreUnique({});
    }
}

} // namespace vex::vk