#pragma once

#include <Vex/QueueType.h>
#include <Vex/Utility/UniqueHandle.h>
#include <Vex/Platform/PlatformWindow.h>

#include <RHI/RHIFwd.h>
#include <RHI/RHISwapChain.h>

#include <DX12/DX12Headers.h>

namespace vex
{
struct PlatformWindow;
} // namespace vex

namespace vex::dx12
{

class DX12SwapChain final : public RHISwapChainBase
{
public:
    DX12SwapChain(ComPtr<DX12Device>& device,
                  SwapChainDesc& desc,
                  const ComPtr<ID3D12CommandQueue>& graphicsCommandQueue,
                  const PlatformWindow& platformWindow);
    ~DX12SwapChain();

    virtual void RecreateSwapChain(u32 width, u32 height) override;

    virtual TextureDesc GetBackBufferTextureDescription() const override;

    virtual bool NeedsRecreation() const override;

    virtual ColorSpace GetValidColorSpace(ColorSpace preferredColorSpace) const override;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) override;
    virtual SyncToken Present(u8 frameIndex,
                              RHI& rhi,
                              NonNullPtr<RHICommandList> commandList,
                              bool isFullscreen) override;

private:
    DXGI_FORMAT GetDXGIFormat() const;
    DXGI_OUTPUT_DESC1 GetBestOutputDesc() const;

    void ApplyColorSpace() const;

    static u8 GetBackBufferCount(FrameBuffering frameBuffering);

    ComPtr<DX12Device> device;
    NonNullPtr<SwapChainDesc> desc;
    ComPtr<ID3D12CommandQueue> graphicsCommandQueue;
    ComPtr<IDXGISwapChain4> swapChain;

    PlatformWindowHandle windowHandle;
};

} // namespace vex::dx12