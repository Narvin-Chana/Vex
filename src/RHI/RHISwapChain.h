#pragma once

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/NonNullPtr.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct SyncToken;
struct TextureDescription;

struct SwapChainDescription
{
    TextureFormat format;
    FrameBuffering frameBuffering;
    bool useVSync = false;
};

class RHISwapChainBase
{
public:
    virtual void Resize(u32 width, u32 height) = 0;

    virtual TextureDescription GetBackBufferTextureDescription() const = 0;

    // Could lead to recreating the swapchain (eg: for Vulkan).
    virtual void SetVSync(bool enableVSync) = 0;
    virtual bool NeedsFlushForVSyncToggle() = 0;

    // FrameIndex is used for our per-frame resources (semaphores etc..).
    // The actual backbuffer returned will be determined by the swapChain's internal index.
    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) = 0;
    virtual SyncToken Present(u8 frameIndex, RHI& rhi, NonNullPtr<RHICommandList> commandList, bool isFullscreen) = 0;
};

} // namespace vex