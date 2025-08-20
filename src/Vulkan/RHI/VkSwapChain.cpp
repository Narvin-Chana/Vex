#include "VkSwapChain.h"

#include <algorithm>
#include <utility>

#include <Vex/PlatformWindow.h>

#include <Vulkan/RHI/VkTexture.h>
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
                         const SwapChainDescription& description,
                         const PlatformWindow& platformWindow)
    : ctx{ ctx }
    , description{ description }
{
    VEX_ASSERT(IsSwapChainSupported(ctx->physDevice, ctx->surface));

    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);
    surfaceFormat = GetBestSurfaceFormat(supportDetails, TextureFormatToVulkan(description.format));
    presentMode = GetBestPresentMode(supportDetails, description.useVSync);

    u32 maxSupportedImageCount =
        std::max(supportDetails.capabilities.minImageCount + 1, supportDetails.capabilities.maxImageCount);
    u8 requestedImageCount = std::to_underlying(description.frameBuffering);

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

RHITexture VkSwapChain::CreateBackBuffer(u8 backBufferIndex)
{
    auto backbufferImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);

    TextureDescription desc{ .name = std::format("backbuffer_{}", backBufferIndex),
                             .type = TextureType::Texture2D,
                             .width = width,
                             .height = height,
                             .depthOrArraySize = 1,
                             .mips = 1,
                             .format = VulkanToTextureFormat(surfaceFormat.format),
                             .usage = TextureUsage::RenderTarget | TextureUsage::ShaderRead };

    return { ctx, std::move(desc), backbufferImages[backBufferIndex] };
}

void VkSwapChain::InitSwapchainResource(u32 inWidth, u32 inHeight)
{
    width = inWidth;
    height = inHeight;
    ::vk::Extent2D extent = GetBestSwapExtent(supportDetails, width, height);

    ::vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface = ctx->surface,
                                                      .minImageCount = std::to_underlying(description.frameBuffering),
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