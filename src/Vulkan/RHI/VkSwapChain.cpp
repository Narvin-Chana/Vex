#include "VkSwapChain.h"

#include <algorithm>
#include <utility>

#include <Vex/Platform/PlatformWindow.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIFence.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Synchronization.h>
#include <Vex/Utility/Formattable.h>

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
    // See for more info: https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html
    if (useVSync)
    {
        // with VSync we want to use eFifo or eMailbox which do not cause tearing to the screen
        // eMailbox is a bit better as it allows the app to keep rendering and always keeps the latest frame
        // whereas eFifo keeps the order and forces a wait before presenting if the queue is full
        auto it = std::ranges::find(details.presentModes, ::vk::PresentModeKHR::eMailbox);
        return it != details.presentModes.end() ? *it : ::vk::PresentModeKHR::eFifo;
    }

    // If not using VSync, we use eImmediate which never waits for the present to complete to swap the images.
    return ::vk::PresentModeKHR::eImmediate;
}

static std::vector<::vk::PresentModeKHR> GetCompatiblePresentModes(::vk::PhysicalDevice device,
                                                                   ::vk::SurfaceKHR surface,
                                                                   ::vk::PresentModeKHR presentMode)
{
    ::vk::SurfacePresentModeEXT surfacePresentMode{
        .presentMode = presentMode,
    };

    ::vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo{ .pNext = &surfacePresentMode, .surface = surface };

    ::vk::SurfacePresentModeCompatibilityKHR modes{
        .presentModeCount = 0,
    };
    ::vk::SurfaceCapabilities2KHR surfaceCaps{
        .pNext = &modes,
    };

    // Query once to know the count, then query again to fill in the vector of present modes.
    VEX_VK_CHECK << device.getSurfaceCapabilities2KHR(&surfaceInfo, &surfaceCaps);
    std::vector<::vk::PresentModeKHR> compatiblePresentModes(modes.presentModeCount);
    modes.pPresentModes = compatiblePresentModes.data();
    VEX_VK_CHECK << device.getSurfaceCapabilities2KHR(&surfaceInfo, &surfaceCaps);
    return compatiblePresentModes;
}

static ::vk::UniqueSemaphore CreateBinarySemaphore(NonNullPtr<VkGPUContext> ctx)
{
    ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eBinary };
    auto semaphore = VEX_VK_CHECK <<= ctx->device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
        .pNext = &createInfo,
    });
    return semaphore;
}

VkSwapChain::VkSwapChain(NonNullPtr<VkGPUContext> ctx, SwapChainDesc& desc, const PlatformWindow& platformWindow)
    : ctx{ ctx }
    , desc{ desc }
{
    VEX_ASSERT(IsSwapChainSupported(ctx->physDevice, ctx->surface));

    RecreateSwapChain();

    const u32 maxSupportedImageCount =
        std::max(supportDetails.capabilities.minImageCount + 1, supportDetails.capabilities.maxImageCount);
    const u8 requestedImageCount = std::to_underlying(desc.frameBuffering);
    // Need to have at least the requested amount of swap chain images
    VEX_ASSERT(maxSupportedImageCount >= requestedImageCount);

    std::ranges::generate_n(std::back_inserter(backbufferAcquisition),
                            requestedImageCount,
                            [ctx] { return CreateBinarySemaphore(ctx); });
}

void VkSwapChain::RecreateSwapChain()
{
    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);
    currentColorSpace = GetValidColorSpace(desc->preferredColorSpace);
    surfaceFormat = GetBestSurfaceFormat(supportDetails);

    desiredPresentMode = GetBestPresentMode(supportDetails, desc->useVSync);
    compatiblePresentModes = GetCompatiblePresentModes(ctx->physDevice, ctx->surface, desiredPresentMode);

    if (supportDetails.capabilities.currentExtent.width == 0 || supportDetails.capabilities.currentExtent.height == 0)
    {
        return;
    }

    if (desc->useHDRIfSupported && currentColorSpace != desc->preferredColorSpace)
    {
        VEX_LOG(Warning,
                "The user-preferred swapchain color space ({}) is not supported by your current display. Falling back "
                "to format {} "
                "with color space {} instead. Present mode: {}.",
                desc->preferredColorSpace,
                surfaceFormat.format,
                currentColorSpace,
                desiredPresentMode);
    }

    // Indicate which present modes to use, this is possible due to VK_EXT_swapchain_maintenance1 and allows for
    // changing the present mode without recreating the swapchain.
    ::vk::SwapchainPresentModesCreateInfoKHR presentModesCreateInfo{
        .presentModeCount = static_cast<u32>(compatiblePresentModes.size()),
        .pPresentModes = compatiblePresentModes.data(),
    };

    const bool needsConcurrent = ctx->queueFamilyIndices.size() > 1;
    ::vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .pNext = &presentModesCreateInfo,
        .surface = ctx->surface,
        .minImageCount = std::to_underlying(desc->frameBuffering),
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = supportDetails.capabilities.currentExtent,
        .imageArrayLayers = 1,
        // Only use transfer usages for swapchain. We only copy to it (and need transfer src for presentation).
        .imageUsage = ::vk::ImageUsageFlagBits::eTransferDst | ::vk::ImageUsageFlagBits::eTransferSrc,
        .imageSharingMode = needsConcurrent ? ::vk::SharingMode::eConcurrent : ::vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = needsConcurrent ? static_cast<u32>(ctx->queueFamilyIndices.size()) : 0,
        .pQueueFamilyIndices = needsConcurrent ? ctx->queueFamilyIndices.data() : nullptr,
        .preTransform = supportDetails.capabilities.currentTransform,
        .compositeAlpha = ::vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = desiredPresentMode,
        .clipped = ::vk::True,
        .oldSwapchain = swapchain ? *swapchain : nullptr,
    };

    if (swapchain)
    {
        pendingOldSwapchains.push_back(std::move(swapchain));
    }

    swapchain = VEX_VK_CHECK <<= ctx->device.createSwapchainKHRUnique(swapChainCreateInfo);

    auto newImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);
    if (newImages.size() != swapChainCreateInfo.minImageCount)
    {
        VEX_LOG(Warning, "Swapchain returned more images than requested for. This might cause instabilities");
    }

    // We're no longer out of date.
    swapchainIsInErrorState = false;
}

bool VkSwapChain::NeedsRecreation() const
{
    // Update support details.
    const ::vk::PresentModeKHR newPresentMode = GetBestPresentMode(supportDetails, desc->useVSync);
    // Only recreate if the new present mode is different to the previous and is not in the current swapchain's
    // compatible present modes. Thanks to VK_EXT_swapchain_maintenance1 we can avoid a bunch of swapchain recreations
    // when changing the presentation mode (and thus when changing if vsync is enabled).
    const bool needsRecreationDueToPresentMode =
        newPresentMode != desiredPresentMode &&
        std::ranges::find(compatiblePresentModes, newPresentMode) == compatiblePresentModes.end();

    return swapchainIsInErrorState || needsRecreationDueToPresentMode || !IsColorSpaceStillSupported(*desc) ||
           (!desc->useHDRIfSupported && IsHDREnabled());
}

bool VkSwapChain::CanRecreate()
{
    supportDetails = GetSwapChainSupportDetails(ctx->physDevice, ctx->surface);

    return NeedsRecreation() && supportDetails.capabilities.currentExtent.width != 0 &&
           supportDetails.capabilities.currentExtent.height != 0;
}

TextureDesc VkSwapChain::GetBackBufferTextureDescription() const
{
    return TextureDesc{
        .name = "backbuffer",
        .type = TextureType::Texture2D,
        .format = VulkanToTextureFormat(surfaceFormat.format),
        .width = supportDetails.capabilities.currentExtent.width,
        .height = supportDetails.capabilities.currentExtent.height,
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

std::optional<RHITexture> VkSwapChain::AcquireBackBuffer(u8 frameIndex, RHI& rhi)
{
    pendingOldSwapchains.clear();
    ProcessPresentHistory();

    // Update the desired present mode, and if necessary, mark the swapchain as being in an errored state.
    desiredPresentMode = GetBestPresentMode(supportDetails, desc->useVSync);
    if (std::ranges::find(compatiblePresentModes, desiredPresentMode) == compatiblePresentModes.end())
    {
        RecreateSwapChain();
    }

    // Acquire the next backbuffer image.
    auto res = ctx->device.acquireNextImageKHR(*swapchain,
                                               std::numeric_limits<u64>::max(),
                                               *backbufferAcquisition[frameIndex],
                                               VK_NULL_HANDLE,
                                               &currentBackbufferId);
    if (res == ::vk::Result::eErrorOutOfDateKHR || res == ::vk::Result::eSuboptimalKHR)
    {
        swapchainIsInErrorState = true;
        return std::nullopt;
    }

    VEX_VK_CHECK << res;

    // Return the acquired backbuffer.
    auto backbufferImages = VEX_VK_CHECK <<= ctx->device.getSwapchainImagesKHR(*swapchain);
    TextureDesc bbDesc = GetBackBufferTextureDescription();
    bbDesc.name = std::format("backbuffer_{}", currentBackbufferId);
    return RHITexture{ ctx, std::move(bbDesc), backbufferImages[currentBackbufferId] };
}

SyncToken VkSwapChain::Present(u8 frameIndex, RHI& rhi, NonNullPtr<RHICommandList> commandList)
{
    ::vk::CommandBufferSubmitInfo cmdBufferSubmitInfo{ .commandBuffer = commandList->GetNativeCommandList() };

    // Before rendering the graphics queue, we must wait for acquireImage to be done.
    // This equates to waiting on the backbufferAcquisition binary semaphore of this backbuffer.
    ::vk::SemaphoreSubmitInfo acquireWaitInfo = {
        .semaphore = *backbufferAcquisition[frameIndex],
        .stageMask = ::vk::PipelineStageFlagBits2::eAllCommands,
    };

    ::vk::UniqueSemaphore presentSemaphore = GetSemaphore();

    // Signal the present binary semaphore (we only want to present once rendering work is done, in this case the copy
    // to the backbuffer).
    ::vk::SemaphoreSubmitInfo presentSignalInfo = {
        .semaphore = *presentSemaphore,
        .stageMask = ::vk::PipelineStageFlagBits2::eAllCommands,
    };

    SyncToken syncToken =
        rhi.SubmitToQueue(commandList->GetQueue(), { cmdBufferSubmitInfo }, { acquireWaitInfo }, { presentSignalInfo });

    // Present now that we've submitted the present copy work.
    ::vk::UniqueFence presentFence = GetPresentFence();

    ::vk::SwapchainPresentModeInfoKHR presentModeInfo{
        .swapchainCount = 1,
        .pPresentModes = &desiredPresentMode,
    };
    ::vk::SwapchainPresentFenceInfoKHR presentFenceInfo{
        .pNext = &presentModeInfo,
        .swapchainCount = 1,
        .pFences = &*presentFence,
    };

    ::vk::PresentInfoKHR presentInfo{
        .pNext = &presentFenceInfo,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*presentSemaphore,
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

    AddPresentToHistory(std::move(presentFence), std::move(presentSemaphore));

    return syncToken;
}

::vk::SurfaceFormatKHR VkSwapChain::GetBestSurfaceFormat(const VkSwapChainSupportDetails& details) const
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

::vk::UniqueFence VkSwapChain::GetPresentFence()
{
    if (!presentFencesPool.empty())
    {
        ::vk::UniqueFence fence = std::move(presentFencesPool.back());
        presentFencesPool.pop_back();
        return fence;
    }
    return VEX_VK_CHECK <<= ctx->device.createFenceUnique(::vk::FenceCreateInfo{});
}

void VkSwapChain::AddPresentToHistory(::vk::UniqueFence fence, ::vk::UniqueSemaphore semaphore)
{
    presentHistory.push_back({ .presentSemaphore = std::move(semaphore),
                               .presentFence = std::move(fence),
                               .oldSwapchains = std::move(pendingOldSwapchains) });
    pendingOldSwapchains = std::vector<::vk::UniqueSwapchainKHR>{};
}

void VkSwapChain::ProcessPresentHistory()
{
    std::erase_if(presentHistory,
                  [&](PresentHistoryEntry& entry)
                  {
                      if (ctx->device.getFenceStatus(*entry.presentFence) == ::vk::Result::eSuccess)
                      {
                          VEX_VK_CHECK << ctx->device.resetFences({ *entry.presentFence });

                          // Recycle the fences and semaphores.
                          presentFencesPool.push_back(std::move(entry.presentFence));
                          presentSemaphorePool.push_back(std::move(entry.presentSemaphore));

                          // Old swapchains are no longer in use, we can safely destroy them.
                          entry.oldSwapchains.clear();

                          return true;
                      }
                      return false;
                  });
}

::vk::UniqueSemaphore VkSwapChain::GetSemaphore()
{
    if (!presentSemaphorePool.empty())
    {
        auto semaphore = std::move(presentSemaphorePool.back());
        presentSemaphorePool.pop_back();
        return semaphore;
    }
    return CreateBinarySemaphore(ctx);
}

} // namespace vex::vk