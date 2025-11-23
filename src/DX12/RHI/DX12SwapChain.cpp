#include "DX12SwapChain.h"

#include <Vex/Formattable.h>
#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Synchronization.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12PhysicalDevice.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

namespace DX12SwapChain_Private
{

static ColorSpace DXGIToColorSpace(DXGI_COLOR_SPACE_TYPE dxgiColorSpace)
{
    switch (dxgiColorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        return ColorSpace::HDR10;
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        return ColorSpace::scRGB;
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
    default:
        return ColorSpace::sRGB;
    }
}

static DXGI_COLOR_SPACE_TYPE ColorSpaceToDXGI(ColorSpace colorSpace)
{
    switch (colorSpace)
    {
    case ColorSpace::HDR10:
        return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    case ColorSpace::scRGB:
        return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    case ColorSpace::sRGB:
    default:
        return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    }
}

} // namespace DX12SwapChain_Private

DX12SwapChain::DX12SwapChain(ComPtr<DX12Device>& device,
                             SwapChainDesc& desc,
                             const ComPtr<ID3D12CommandQueue>& graphicsCommandQueue,
                             const PlatformWindow& platformWindow)
    : device{ device }
    , desc(desc)
    , graphicsCommandQueue(graphicsCommandQueue)
    , windowHandle(platformWindow.windowHandle)
{
    RecreateSwapChain(platformWindow.width, platformWindow.height);
}

DX12SwapChain::~DX12SwapChain() = default;

TextureDesc DX12SwapChain::GetBackBufferTextureDescription() const
{
    return const_cast<DX12SwapChain&>(*this).AcquireBackBuffer(0)->GetDesc();
}

bool DX12SwapChain::NeedsRecreation() const
{
    return
        // Recreate the swapchain if the current swapchain's color space no longer matches the color space of the
        // output.
        !IsColorSpaceStillSupported() ||
        // Or if we're outputting as HDR, but the swapchain desc no longer allows this.
        (!desc->useHDRIfSupported && IsHDREnabled());
}

void DX12SwapChain::RecreateSwapChain(u32 width, u32 height)
{
    currentColorSpace = GetValidColorSpace(desc->preferredColorSpace);
    format = ColorSpaceToSwapChainFormat(currentColorSpace, desc->useHDRIfSupported);

    if (!desc->useHDRIfSupported || currentColorSpace == desc->preferredColorSpace)
    {
        VEX_LOG(Info, "SwapChain uses the format ({}) with color space {}.", format, currentColorSpace);
    }
    else
    {
        VEX_LOG(Warning,
                "The user-preferred swapchain color space ({}) is not supported by your current display. Falling back "
                "to format {} "
                "with color space {} instead.",
                desc->preferredColorSpace,
                format,
                currentColorSpace);
    }

    DXGI_FORMAT nativeFormat = GetDXGIFormat();

    // First time we have to create it from scratch, all subsequent times we can just call resize.
    if (!swapChain)
    {
        DXGI_SWAP_CHAIN_DESC1 nativeSwapChainDesc{
            .Width = width,
            .Height = height,
            .Format = nativeFormat,
            .Stereo = false,
            .SampleDesc = { .Count = 1, .Quality = 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = GetBackBufferCount(desc->frameBuffering),
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
        };
        swapChain = DXGIFactory::CreateSwapChain(nativeSwapChainDesc, graphicsCommandQueue, windowHandle.window);
    }
    else
    {
        chk << swapChain->ResizeBuffers(GetBackBufferCount(desc->frameBuffering),
                                        width,
                                        height,
                                        nativeFormat,
                                        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    }

    ApplyColorSpace();
}

bool DX12SwapChain::IsHDREnabled() const
{
    return currentColorSpace != ColorSpace::sRGB;
}

bool DX12SwapChain::IsColorSpaceStillSupported() const
{
    // Compare the output's actual color space to what we're currently using.
    bool matches = false;
    DXGI_OUTPUT_DESC1 outputDesc = GetBestOutputDesc();

    ColorSpace outputColorSpace = DX12SwapChain_Private::DXGIToColorSpace(outputDesc.ColorSpace);

    // Check if the display's HDR mode matches what we need
    bool outputSupportsHDR = (outputColorSpace != ColorSpace::sRGB);
    bool currentIsHDR = (currentColorSpace != ColorSpace::sRGB);
    bool preferredIsHDR = (desc->preferredColorSpace != ColorSpace::sRGB);

    // If HDR capability changed (HDR<->SDR), we must recreate
    if (outputSupportsHDR != currentIsHDR && preferredIsHDR)
    {
        VEX_LOG(Info,
                "Display HDR state changed (output supports HDR: {}, current uses HDR: {})",
                outputSupportsHDR,
                currentIsHDR);
        return false;
    }

    // If both are HDR, check if the output's specific format matches user preference
    // and differs from what we're using
    if (outputSupportsHDR)
    {

        // If user prefers a specific HDR format and output now matches it,
        // but we're using something different, recreate
        if (preferredIsHDR && outputColorSpace == desc->preferredColorSpace &&
            currentColorSpace != desc->preferredColorSpace)
        {
            VEX_LOG(Info,
                    "Output now supports preferred HDR format {} (currently using {})",
                    desc->preferredColorSpace,
                    currentColorSpace);
            return false;
        }
    }

    // Still compatible
    return true;
}

ColorSpace DX12SwapChain::GetValidColorSpace(ColorSpace preferredColorSpace) const
{
    if (!desc->useHDRIfSupported)
    {
        return ColorSpace::sRGB;
    }

    // If the preferred is not supported, instead fallback to the output's recommended color space.
    const DXGI_OUTPUT_DESC1 outputDesc = GetBestOutputDesc();
    const ColorSpace recommendedColorSpace = DX12SwapChain_Private::DXGIToColorSpace(outputDesc.ColorSpace);
    const bool isHDRColorSpace = recommendedColorSpace != ColorSpace::sRGB;
    // HDR color space being available means we can use any user-preferred color space.
    if (isHDRColorSpace)
    {
        return preferredColorSpace;
    }
    return ColorSpace::sRGB;
}

std::optional<RHITexture> DX12SwapChain::AcquireBackBuffer(u8 frameIndex)
{
    u32 backBufferIndex = swapChain->GetCurrentBackBufferIndex();
    ComPtr<ID3D12Resource> backBuffer;
    chk << swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer));
    return DX12Texture(device, std::format("BackBuffer_{}", backBufferIndex), backBuffer);
}

SyncToken DX12SwapChain::Present(u8 frameIndex, RHI& rhi, NonNullPtr<RHICommandList> commandList, bool isFullscreen)
{
    // Ignore the SyncToken of this function.
    // We instead return the sync token post-present.
    rhi.Submit({ &commandList, 1 }, {});

    chk << swapChain->Present(desc->useVSync, (!desc->useVSync && !isFullscreen) ? DXGI_PRESENT_ALLOW_TEARING : 0);

    auto& fence = (*rhi.fences)[QueueType::Graphics];
    u64 signalValue = fence.nextSignalValue++;
    chk << rhi.GetNativeQueue(QueueType::Graphics)->Signal(fence.fence.Get(), signalValue);

    return { QueueType::Graphics, signalValue };
}

DXGI_FORMAT DX12SwapChain::GetDXGIFormat() const
{
    auto ValidateFlipModelSupportedFormats = [](DXGI_FORMAT format)
    {
        return
            // sRGB color space
            format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            // scRGB color space
            format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
            // HDR10 color space
            format == DXGI_FORMAT_R10G10B10A2_UNORM;
    };

    // DX12's preference for swapchains is to use the non-srgb format, and then use SRGB format when creating views.
    static constexpr bool AllowSRGBSwapChainFormat = false;
    DXGI_FORMAT nativeFormat = TextureFormatToDXGI(format, AllowSRGBSwapChainFormat);

    if (!ValidateFlipModelSupportedFormats(nativeFormat))
    {
        VEX_LOG(Fatal, "Invalid swapchain format for the _FLIP_ swap mode.");
    }

    return nativeFormat;
}

DXGI_OUTPUT_DESC1 DX12SwapChain::GetBestOutputDesc() const
{
    // This code was inspired by the DirectX-Graphics sample for HDR rendering (D3D12HDR).

    // IsCurrent returns false when the monitor's color state has changed (eg: unplugging monitor, changing OS HDR
    // setting).
    if (!DXGIFactory::dxgiFactory->IsCurrent())
    {
        // Recreate the factory.
        DXGIFactory::InitializeDXGIFactory();
    }

    DX12PhysicalDevice& physDevice = static_cast<DX12PhysicalDevice&>(*GPhysicalDevice);

    // Get a fresh adapter from the current factory, not the cached one.
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIAdapter4> adapter4;

    // Re-enumerate to get the current adapter
    for (UINT adapterIndex = 0; DXGIFactory::dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Find the same adapter (match by LUID or device ID).
        DXGI_ADAPTER_DESC1 cachedDesc;
        physDevice.adapter->GetDesc1(&cachedDesc);

        if (desc.AdapterLuid.LowPart == cachedDesc.AdapterLuid.LowPart &&
            desc.AdapterLuid.HighPart == cachedDesc.AdapterLuid.HighPart)
        {
            // This is our adapter, get the IDXGIAdapter4 interface.
            adapter.As(&adapter4);
            break;
        }
    }

    // If we didn't find it via enumeration, fall back to the cached adapter (shouldn't happen).
    if (!adapter4)
    {
        adapter4 = physDevice.adapter;
    }
    // Otherwise, update the adapter.
    else
    {
        static_cast<DX12PhysicalDevice&>(*GPhysicalDevice).adapter = adapter4;
    }

    auto ComputeIntersectionArea = [](i32 ax1, i32 ay1, i32 ax2, i32 ay2, i32 bx1, i32 by1, i32 bx2, i32 by2) -> i32
    {
        return std::max(0, std::min(ax2, bx2) - std::max(ax1, bx1)) *
               std::max(0, std::min(ay2, by2) - std::max(ay1, by1));
    };

    RECT windowBounds;
    GetWindowRect(windowHandle.window, &windowBounds);

    // Get the retangle bounds of the app window
    i32 ax1 = windowBounds.left;
    i32 ay1 = windowBounds.top;
    i32 ax2 = windowBounds.right;
    i32 ay2 = windowBounds.bottom;

    u32 i = 0;
    ComPtr<IDXGIOutput> currentOutput;
    ComPtr<IDXGIOutput> bestOutput;
    float bestIntersectArea = -1;
    while (adapter4->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
    {
        // Get the rectangle bounds of current output
        DXGI_OUTPUT_DESC desc{};
        chk << currentOutput->GetDesc(&desc);
        RECT r = desc.DesktopCoordinates;
        i32 bx1 = r.left;
        i32 by1 = r.top;
        i32 bx2 = r.right;
        i32 by2 = r.bottom;

        // Compute the intersection
        i32 intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
        if (intersectArea > bestIntersectArea)
        {
            bestOutput = currentOutput;
            bestIntersectArea = static_cast<float>(intersectArea);
        }

        ++i;
    }

    ComPtr<IDXGIOutput6> output6;
    chk << bestOutput.As(&output6);
    DXGI_OUTPUT_DESC1 desc{};
    chk << output6->GetDesc1(&desc);

    return desc;
}

void DX12SwapChain::ApplyColorSpace() const
{
    DXGI_COLOR_SPACE_TYPE dxgiColorSpace = DX12SwapChain_Private::ColorSpaceToDXGI(currentColorSpace);
    chk << swapChain->SetColorSpace1(dxgiColorSpace);
}

u8 DX12SwapChain::GetBackBufferCount(FrameBuffering frameBuffering)
{
    return std::max<u8>(2, std::to_underlying(frameBuffering));
}

} // namespace vex::dx12