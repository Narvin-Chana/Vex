#pragma once

#include <optional>
#include <vector>

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/NonNullPtr.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct SyncToken;
struct TextureDesc;

enum class ColorSpace : u8
{
    sRGB,  // Rec709 standard HDR.
    scRGB, // Extended linear sRGB.
    HDR10, // Rec2020 with ST2084 (or PQ / Perceptual Quantizer) transfer function.
    // P3,    // Dolby Vision (unsupported for now).
};

// Converts the color space to its appropriate swapchain format.
// The caller must first make sure that the user's output display correctly supports the color space.
inline TextureFormat ColorSpaceToSwapChainFormat(ColorSpace colorSpace, bool allowHDR)
{
    if (!allowHDR)
    {
        return TextureFormat::BGRA8_UNORM;
        // TODO: Also do RGBA8 handling!
    }

    switch (colorSpace)
    {
    case ColorSpace::sRGB:
    {
        // Typical sRGB requires no specific handling (is a non-HDR color space).
        return TextureFormat::BGRA8_UNORM;
        // TODO: or RGBA8 handling!
    }
    case ColorSpace::scRGB:
    {
        // scRGB uses FP16 format.
        return TextureFormat::RGBA16_FLOAT;
    }
    case ColorSpace::HDR10:
    {
        // HDR10 uses a 10 10 10 2 floating point format.
        return TextureFormat::RGB10A2_UNORM;
    }
    }
    std::unreachable();
}

struct SwapChainDesc
{
    // Determines the minimum number of backbuffers the application will leverage at once.
    FrameBuffering frameBuffering = FrameBuffering::Triple;

    // Enables or disables vertical sync.
    bool useVSync = false;

    // Determines if the swapchain should attempt to use an HDR color space.
    // If the preferred HDR color space is not supported, we will fallback to:
    //   1. The actual HDR color space your display supports (if any).
    //   or 2. The SDR swapchain formats (aka RGBA8_UNORM/BGRA8_UNORM).
    bool useHDRIfSupported = false;
    // Preferred HDR Color Space (only valid if useHDRIfSupported is enabled).
    ColorSpace preferredColorSpace = ColorSpace::sRGB;
};

class RHISwapChainBase
{
public:
    // Recreates the swapchain.
    virtual void RecreateSwapChain(u32 width, u32 height) = 0;
    // Determines if the swapchain is valid or if it needs to be recreated.
    virtual bool NeedsRecreation() const = 0;

    virtual TextureDesc GetBackBufferTextureDescription() const = 0;

    virtual ColorSpace GetValidColorSpace(ColorSpace preferredColorSpace) const = 0;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) = 0;
    virtual SyncToken Present(u8 frameIndex, RHI& rhi, NonNullPtr<RHICommandList> commandList, bool isFullscreen) = 0;

    bool IsHDREnabled() const
    {
        return currentColorSpace != ColorSpace::sRGB;
    }

    bool IsColorSpaceStillSupported(SwapChainDesc& desc) const
    {
        // Determine what the best color space would be given current conditions
        ColorSpace bestColorSpace = GetValidColorSpace(desc.preferredColorSpace);

        // If the best color space differs from what we're currently using, we need to recreate
        return bestColorSpace == currentColorSpace;
    }

    ColorSpace GetCurrentColorSpace() const
    {
        return currentColorSpace;
    }

protected:
    ColorSpace currentColorSpace = ColorSpace::sRGB;
    TextureFormat format = TextureFormat::BGRA8_UNORM;
};

} // namespace vex