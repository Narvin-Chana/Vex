#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/RHIFwd.h>
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
                  const ComPtr<ID3D12CommandQueue>& commandQueue,
                  const PlatformWindow& platformWindow);
    ~DX12SwapChain();
    virtual void Resize(u32 width, u32 height) override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual RHITexture CreateBackBuffer(u8 backBufferIndex) override;

private:
    static u8 GetBackBufferCount(FrameBuffering frameBuffering);

    void Present(bool isFullscreenMode);

    ComPtr<DX12Device> device;
    SwapChainDescription description;
    ComPtr<IDXGISwapChain4> swapChain;

    friend class DX12RHI;
};

} // namespace vex::dx12