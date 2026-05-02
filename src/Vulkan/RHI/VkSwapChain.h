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

    virtual void RecreateSwapChain() override;
    virtual bool NeedsRecreation() const override;
    virtual bool CanRecreate() override;

    virtual TextureDesc GetBackBufferTextureDescription() const override;

    virtual ColorSpace GetValidColorSpace(ColorSpace preferredColorSpace) const override;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex, RHI& rhi) override;
    virtual SyncToken Present(u8 frameIndex,
                              RHI& rhi,
                              NonNullPtr<RHICommandList> commandList) override;

private:
    ::vk::SurfaceFormatKHR GetBestSurfaceFormat(const VkSwapChainSupportDetails& details) const;

    ::vk::UniqueFence GetPresentFence();
    void AddPresentToHistory(::vk::UniqueFence fence, ::vk::UniqueSemaphore semaphore);
    void ProcessPresentHistory();

    ::vk::UniqueSemaphore GetSemaphore();

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<SwapChainDesc> desc;

    VkSwapChainSupportDetails supportDetails;
    std::vector<::vk::PresentModeKHR> compatiblePresentModes;
    ::vk::PresentModeKHR desiredPresentMode;
    ::vk::SurfaceFormatKHR surfaceFormat;

    ::vk::UniqueSwapchainKHR swapchain;

    // Used to wait for acquisition of the next frame's backbuffer image.
    std::vector<::vk::UniqueSemaphore> backbufferAcquisition;

    std::vector<::vk::UniqueSemaphore> presentSemaphorePool;
    std::vector<::vk::UniqueFence> presentFencesPool;

    std::vector<::vk::UniqueSwapchainKHR> pendingOldSwapchains;

    struct PresentHistoryEntry
    {
        ::vk::UniqueSemaphore presentSemaphore;
        ::vk::UniqueFence presentFence;
        std::vector<::vk::UniqueSwapchainKHR> oldSwapchains;
    };
    std::vector<PresentHistoryEntry> presentHistory;

    bool swapchainIsInErrorState = false;

    u32 currentBackbufferId = 0;

    friend class VkRHI;
};

} // namespace vex::vk