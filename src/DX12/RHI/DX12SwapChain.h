#pragma once

#include <Vex/QueueType.h>
#include <RHI/RHIFwd.h>
#include <Vex/UniqueHandle.h>

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
                  SwapChainDescription desc,
                  const ComPtr<ID3D12CommandQueue>& graphicsCommandQueue,
                  const PlatformWindow& platformWindow);
    ~DX12SwapChain();

    virtual void Resize(u32 width, u32 height) override;

    virtual TextureDesc GetBackBufferTextureDescription() const override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual std::optional<RHITexture> AcquireBackBuffer(u8 frameIndex) override;
    virtual SyncToken Present(u8 frameIndex,
                              RHI& rhi,
                              NonNullPtr<RHICommandList> commandList,
                              bool isFullscreen) override;

private:
    static u8 GetBackBufferCount(FrameBuffering frameBuffering);

    ComPtr<DX12Device> device;
    SwapChainDescription desc;
    ComPtr<ID3D12CommandQueue> graphicsCommandQueue;
    ComPtr<IDXGISwapChain4> swapChain;
};

} // namespace vex::dx12