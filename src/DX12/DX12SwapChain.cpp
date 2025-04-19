#include "DX12SwapChain.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <DX12/DX12Formats.h>
#include <DX12/DX12Texture.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12SwapChain::DX12SwapChain(const ComPtr<DX12Device>& device,
                             SwapChainDescription desc,
                             const ComPtr<ID3D12CommandQueue>& commandQueue,
                             const PlatformWindow& platformWindow)
    : description(std::move(desc))
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
    swapChain = DXGIFactory::CreateSwapChain(nativeSwapChainDesc, commandQueue, platformWindow.windowHandle.window);
    ExtractBackBuffers();
}

DX12SwapChain::~DX12SwapChain()
{
}

void DX12SwapChain::Present()
{
    chk << swapChain->Present(description.useVSync, !description.useVSync ? DXGI_PRESENT_ALLOW_TEARING : 0);
}

void DX12SwapChain::Resize(u32 width, u32 height)
{
    backBuffers.clear();
    chk << swapChain->ResizeBuffers(GetBackBufferCount(description.frameBuffering),
                                    width,
                                    height,
                                    DXGI_FORMAT_UNKNOWN,
                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    ExtractBackBuffers();
}

void DX12SwapChain::SetVSync(bool enableVSync)
{
    description.useVSync = enableVSync;
}

bool DX12SwapChain::NeedsFlushForVSyncToggle()
{
    return false;
}

RHITexture* DX12SwapChain::GetBackBuffer(u8 backBufferIndex)
{
    return backBuffers[backBufferIndex].get();
}

void DX12SwapChain::ExtractBackBuffers()
{
    for (u32 backBufferIndex = 0; backBufferIndex < GetBackBufferCount(description.frameBuffering); ++backBufferIndex)
    {
        ComPtr<ID3D12Resource> backBuffer;
        chk << swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer));
        backBuffers.emplace_back(new DX12Texture(std::format("BackBuffer_{}", backBufferIndex), backBuffer));
    }
}

u8 DX12SwapChain::GetBackBufferCount(FrameBuffering frameBuffering)
{
    return std::max<u8>(2, std::to_underlying(frameBuffering));
}

} // namespace vex::dx12