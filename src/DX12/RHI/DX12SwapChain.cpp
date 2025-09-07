#include "DX12SwapChain.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Synchronization.h>

#include <DX12/DX12Formats.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12SwapChain::DX12SwapChain(ComPtr<DX12Device>& device,
                             SwapChainDescription desc,
                             const ComPtr<ID3D12CommandQueue>& graphicsCommandQueue,
                             const PlatformWindow& platformWindow)
    : device{ device }
    , description(std::move(desc))
    , graphicsCommandQueue(graphicsCommandQueue)
{
    auto ValidateFlipModelSupportedFormats = [](DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R10G10B10A2_UNORM;
    };

    DXGI_FORMAT nativeFormat = TextureFormatToDXGI(description.format);
    if (!ValidateFlipModelSupportedFormats(nativeFormat))
    {
        VEX_LOG(Fatal, "Invalid swapchain format for the _FLIP_ swap mode.");
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 nativeSwapChainDesc{
        .Width = platformWindow.width,
        .Height = platformWindow.height,
        .Format = nativeFormat,
        .Stereo = false,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = GetBackBufferCount(description.frameBuffering),
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
    };
    swapChain =
        DXGIFactory::CreateSwapChain(nativeSwapChainDesc, graphicsCommandQueue, platformWindow.windowHandle.window);
}

DX12SwapChain::~DX12SwapChain() = default;

void DX12SwapChain::Resize(u32 width, u32 height)
{
    chk << swapChain->ResizeBuffers(GetBackBufferCount(description.frameBuffering),
                                    width,
                                    height,
                                    // DXGI_FORMAT_UNKNOWN keeps the previous format
                                    DXGI_FORMAT_UNKNOWN,
                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
}

TextureDescription DX12SwapChain::GetBackBufferTextureDescription() const
{
    return const_cast<DX12SwapChain&>(*this).AcquireBackBuffer(0)->GetDescription();
}

void DX12SwapChain::SetVSync(bool enableVSync)
{
    description.useVSync = enableVSync;
}

bool DX12SwapChain::NeedsFlushForVSyncToggle()
{
    return false;
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

    chk << swapChain->Present(description.useVSync,
                              (!description.useVSync && !isFullscreen) ? DXGI_PRESENT_ALLOW_TEARING : 0);

    auto& fence = (*rhi.fences)[CommandQueueType::Graphics];
    u64 signalValue = fence.nextSignalValue++;
    chk << rhi.GetNativeQueue(CommandQueueType::Graphics)->Signal(fence.fence.Get(), signalValue);

    return { CommandQueueType::Graphics, signalValue };
}

u8 DX12SwapChain::GetBackBufferCount(FrameBuffering frameBuffering)
{
    return std::max<u8>(2, std::to_underlying(frameBuffering));
}

} // namespace vex::dx12