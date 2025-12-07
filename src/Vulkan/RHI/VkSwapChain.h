#pragma once

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>
#include <RHI/RHISwapChain.h>

#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct PlatformWindow;
} // namespace vex

namespace vex::vk
{

struct VkSwapChainSupportDetails
{
    ::vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<::vk::SurfaceFormatKHR> formats;
    std::vector<::vk::PresentModeKHR> presentModes;

    bool IsValid() const
    {
        return !formats.empty() && !presentModes.empty();
    }
};

class VkSwapChain final : public RHISwapChainBase
{

public:
    VkSwapChain(NonNullPtr<VkGPUContext> ctx, SwapChainDesc& desc, const PlatformWindow& platformWindow);

    virtual void RecreateSwapChain(u32 width, u32 height) override;
    virtual bool NeedsRecreation() const override;

    virtual TextureDesc GetBackBufferTextureDescription() const override;

    virtual ColorSpace GetValidColorSpace(ColorSpace preferredColorSpace) const override;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) override;
    virtual SyncToken Present(u8 frameIndex,
                              RHI& rhi,
                              NonNullPtr<RHICommandList> commandList,
                              bool isFullscreen) override;

private:
    void InitSwapchainResource(u32 width, u32 height);
    ::vk::SurfaceFormatKHR GetBestSurfaceFormat(const VkSwapChainSupportDetails& details);

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<SwapChainDesc> desc;

    VkSwapChainSupportDetails supportDetails;
    ::vk::PresentModeKHR presentMode;
    ::vk::SurfaceFormatKHR surfaceFormat;

    ::vk::UniqueSwapchainKHR swapchain;

    // Used to wait for acquisition of the next frame's backbuffer image.
    std::vector<::vk::UniqueSemaphore> backbufferAcquisition;
    // Used to wait for all command lists to finish before presenting.
    std::vector<::vk::UniqueSemaphore> presentSemaphore;

    bool swapchainIsInErrorState = false;

    u32 currentBackbufferId;
    u32 width, height;

    friend class VkRHI;
};

} // namespace vex::vk