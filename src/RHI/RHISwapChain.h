#pragma once

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/RHIFwd.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

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

    // Could lead to recreating the swapchain (eg: for Vulkan).
    virtual void SetVSync(bool enableVSync) = 0;
    virtual bool NeedsFlushForVSyncToggle() = 0;

    virtual RHITexture CreateBackBuffer(u8 backBufferIndex) = 0;
};

} // namespace vex