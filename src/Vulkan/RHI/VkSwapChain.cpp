#include "VkSwapChain.h"

#include <algorithm>
#include <utility>

#include <Vex/Utility/Formattable.h>
#include <Vex/Platform/PlatformWindow.h>
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

static ::vk::PresentModeKHR GetBestPresentMode(const VkSwapChainSupportDetails& details, bool useVSync)
{
    // VSync means we should use eFifo which is available on most platforms.
    // Source: https://stackoverflow.com/questions/36896021/enabling-vsync-in-vulkan
    if (!useVSync)
    {
        return ::vk::PresentModeKHR::eFifo;
    }

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

VkSwapChain::VkSwapChain(NonNullPtr<VkGPUContext> ctx, SwapChainDesc& desc, const PlatformWindow& platformWindow)
    : ctx{ ctx }
    , desc{ desc }
{
    VEX_ASSERT(IsSwapChainSupported(ctx->physDevice, ctx->surface));
    RecreateSwapChain(platformWindow.width, platformWindow.height);

    auto BinarySemaphoreCreator = [ctx = ctx]
    {
        ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eBinary };
        return VEX_VK_CHECK <<= ctx->device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
                   .pNext = &createInfo,
               });
    };

    const u32 maxSupportedImageCount =
        std::max(supportDetails.capabilities.minImageCount + 1, supportDetails.capabilities.maxImageCount);
    const u8 requestedImageCount = std::to_underlying(desc.frameBuffering);

    // Need to have at least the requested amount of swap chain images
    VEX_ASSERT(maxSupportedImageCount >= requestedImageCount);

    InitSwapchainResource(platformWindow.width, platformWindow.height);

    auto BinarySemaphoreCreator = [&ctx]
    {
        ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eBinary };
        auto semaphore = VEX_VK_CHECK <<= ctx->device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
            .pNext = &createInfo,
        });
        return semaphore;
    };
    // Requires including the heavy <algorithm>
    std::ranges::generate_n(std::back_inserter(presentSemaphore), requestedImageCount, BinarySemaphoreCreator);
    backbufferAcquisition.resize(requestedImageCount);
}

void VkSwapChain::RecreateSwapChain(u32 width, u32 height)
{
    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);
    currentColorSpace = GetValidColorSpace(desc->preferredColorSpace);
    surfaceFormat = GetBestSurfaceFormat(supportDetails);

    presentMode = GetBestPresentMode(supportDetails, desc->useVSync);

    if (!desc->useHDRIfSupported || currentColorSpace == desc->preferredColorSpace)
    {
        VEX_LOG(Info, "SwapChain uses the format ({}) with color space {}.", surfaceFormat.format, currentColorSpace);
    }
    else
    {
        VEX_LOG(Warning,
                "The user-preferred swapchain color space ({}) is not supported by your current display. Falling back "
                "to format {} "
                "with color space {} instead.",
                desc->preferredColorSpace,
                surfaceFormat.format,
                currentColorSpace);
    }

    InitSwapchainResource(width, height);
}

bool VkSwapChain::NeedsRecreation() const
{
    const ::vk::PresentModeKHR newPresentMode = GetBestPresentMode(supportDetails, desc->useVSync);
    const bool needsRecreationDueToVSync = newPresentMode != presentMode;

    return swapchainIsInErrorState || needsRecreationDueToVSync || !IsColorSpaceStillSupported(*desc) ||
           (!desc->useHDRIfSupported && IsHDREnabled());
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
                        .usage = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
    };
}

ColorSpace VkSwapChain::GetValidColorSpace(ColorSpace preferredColorSpace) const
{
    if (!desc->useHDRIfSupported)
    {
        return ColorSpace::sRGB;
    }

    // Query current surface formats to see what's actually supported
    auto surfaceFormats = VEX_VK_CHECK <<= ctx->physDevice.getSurfaceFormatsKHR(ctx->surface);

    // Helper to check if a specific color space is supported
    auto IsColorSpaceSupported = [&surfaceFormats](::vk::ColorSpaceKHR colorSpace) -> bool
    {
        return std::ranges::any_of(surfaceFormats,
                                   [colorSpace](const ::vk::SurfaceFormatKHR& format)
                                   { return format.colorSpace == colorSpace; });
    };

    // Try to find the preferred color space
    ::vk::ColorSpaceKHR preferredVkColorSpace;
    switch (preferredColorSpace)
    {
    case ColorSpace::HDR10:
        preferredVkColorSpace = ::vk::ColorSpaceKHR::eHdr10St2084EXT;
        break;
    case ColorSpace::scRGB:
        preferredVkColorSpace = ::vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
        break;
    case ColorSpace::sRGB:
    default:
        preferredVkColorSpace = ::vk::ColorSpaceKHR::eSrgbNonlinear;
        break;
    }

    if (IsColorSpaceSupported(preferredVkColorSpace))
    {
        return preferredColorSpace;
    }

    // Fallback: try other HDR formats in order of preference
    if (IsColorSpaceSupported(::vk::ColorSpaceKHR::eHdr10St2084EXT))
    {
        return ColorSpace::HDR10;
    }

    if (IsColorSpaceSupported(::vk::ColorSpaceKHR::eExtendedSrgbLinearEXT))
    {
        return ColorSpace::scRGB;
    }

    // Final fallback to sRGB
    return ColorSpace::sRGB;
}

std::optional<RHITexture> VkSwapChain::AcquireBackBuffer(u8 frameIndex)
{
    // Acquire the next backbuffer image.
    ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eBinary };

    ::vk::UniqueSemaphore acquireSemaphore = VEX_VK_CHECK <<=
        ctx->device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
            .pNext = &createInfo,
        });

    auto res = ctx->device.acquireNextImageKHR(*swapchain,
                                               std::numeric_limits<u64>::max(),
                                               *acquireSemaphore,
                                               VK_NULL_HANDLE,
                                               &currentBackbufferId);
    if (res == ::vk::Result::eErrorOutOfDateKHR || res == ::vk::Result::eSuboptimalKHR)
    {
        swapchainIsInErrorState = true;
        return {};
    }

    backbufferAcquisition[currentBackbufferId] = std::move(acquireSemaphore);

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
        .semaphore = *backbufferAcquisition[currentBackbufferId],
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
    if (res == ::vk::Result::eErrorOutOfDateKHR || res == ::vk::Result::eSuboptimalKHR)
    {
        swapchainIsInErrorState = true;
    }
    else
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

    ::vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface = ctx->surface,
        .minImageCount = std::to_underlying(desc->frameBuffering),
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = ::vk::ImageUsageFlagBits::eColorAttachment | ::vk::ImageUsageFlagBits::eTransferDst |
                      ::vk::ImageUsageFlagBits::eTransferSrc,
        .imageSharingMode = ::vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = supportDetails.capabilities.currentTransform,
        .compositeAlpha = ::vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = ::vk::True,
        .oldSwapchain = swapchain ? *swapchain : nullptr,
    };

    swapchain = VEX_VK_CHECK <<= ctx->device.createSwapchainKHRUnique(swapChainCreateInfo);

    auto newImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);
    if (newImages.size() != swapChainCreateInfo.minImageCount)
    {
        VEX_LOG(Warning, "Swapchain returned more images than requested for. This might cause instabilities");
    }

    // We're no longer out of date.
    swapchainIsInErrorState = false;
}

::vk::SurfaceFormatKHR VkSwapChain::GetBestSurfaceFormat(const VkSwapChainSupportDetails& details)
{
    ::vk::Format requestedFormat =
        TextureFormatToVulkan(ColorSpaceToSwapChainFormat(currentColorSpace, desc->useHDRIfSupported), false);

    ::vk::ColorSpaceKHR requestedColorSpace;
    switch (currentColorSpace)
    {
    case ColorSpace::HDR10:
        requestedColorSpace = ::vk::ColorSpaceKHR::eHdr10St2084EXT;
        break;
    case ColorSpace::scRGB:
        // Not 100% sure about if this is the one that maps to what we want (especially with the Linear/NonLinear
        // aspect).
        requestedColorSpace = ::vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
        break;
    case ColorSpace::sRGB:
    default:
        requestedColorSpace = ::vk::ColorSpaceKHR::eSrgbNonlinear;
        break;
    }

    for (const ::vk::SurfaceFormatKHR& availableFormat : details.formats)
    {
        if (availableFormat.format == requestedFormat && availableFormat.colorSpace == requestedColorSpace)
        {
            return availableFormat;
        }
    }

    VEX_LOG(Fatal, "Format \"{}\" not supported", ::vk::to_string(requestedFormat));
    std::unreachable();
}

} // namespace vex::vk