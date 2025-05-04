#pragma once

#include <Vex/RHI/RHISwapChain.h>

#include "VkGPUContext.h"
#include "VkHeaders.h"
#include "VkTexture.h"

namespace vex
{
class RHICommandList;
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

class VkSwapChain : public RHISwapChain
{

public:
    VkSwapChain(VkGPUContext& ctx, const SwapChainDescription& description, const PlatformWindow& platformWindow);

    virtual void AcquireNextBackbuffer(u8 frameIndex) override;
    virtual void Present() override;
    virtual void Resize(u32 width, u32 height) override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual RHITexture* GetBackBuffer(u8 backBufferIndex) override;

private:
    void InitSwapchainResource(u32 width, u32 height);

    VkSwapChainSupportDetails supportDetails;
    ::vk::PresentModeKHR presentMode;
    ::vk::SurfaceFormatKHR surfaceFormat;

    SwapChainDescription description;

    ::vk::UniqueSwapchainKHR swapchain;
    std::vector<UniqueHandle<VkBackbufferTexture>> backbufferImages;
    std::vector<::vk::UniqueImageView> backbufferViews;
    std::vector<::vk::UniqueSemaphore> presentSemaphore;

    std::vector<::vk::UniqueSemaphore> backbufferAcquisition;

    u32 currentBackbufferId;
    u32 width, height;

    VkGPUContext& ctx;
};

} // namespace vex::vk