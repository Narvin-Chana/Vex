#pragma once

#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>

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

    bool IsValid() const noexcept
    {
        return !formats.empty() && !presentModes.empty();
    }
};

class VkSwapChain final : public RHISwapChainBase
{

public:
    VkSwapChain(NonNullPtr<VkGPUContext> ctx,
                const SwapChainDescription& description,
                const PlatformWindow& platformWindow);

    virtual void Resize(u32 width, u32 height) override;

    virtual TextureDescription GetBackBufferTextureDescription() const override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) override;
    virtual SyncToken Present(u8 frameIndex,
                              RHI& rhi,
                              NonNullPtr<RHICommandList> commandList,
                              bool isFullscreen) override;

private:
    void InitSwapchainResource(u32 width, u32 height);

    NonNullPtr<VkGPUContext> ctx;

    VkSwapChainSupportDetails supportDetails;
    ::vk::PresentModeKHR presentMode;
    ::vk::SurfaceFormatKHR surfaceFormat;

    SwapChainDescription description;

    ::vk::UniqueSwapchainKHR swapchain;

    // Used to wait for acquisition of the next frame's backbuffer image.
    std::vector<::vk::UniqueSemaphore> backbufferAcquisition;
    // Used to wait for all command lists to finish before presenting.
    std::vector<::vk::UniqueSemaphore> presentSemaphore;

    u32 currentBackbufferId;
    u32 width, height;

    friend class VkRHI;
};

} // namespace vex::vk