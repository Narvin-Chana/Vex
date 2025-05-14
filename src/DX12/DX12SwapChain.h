#pragma once

#include <vector>

#include <Vex/RHI/RHISwapChain.h>
#include <Vex/UniqueHandle.h>

#include <DX12/DX12Headers.h>

namespace vex
{
struct PlatformWindow;
class RHITexture;
} // namespace vex

namespace vex::dx12
{

class DX12SwapChain : public RHISwapChain
{
public:
    DX12SwapChain(const ComPtr<DX12Device>& device,
                  SwapChainDescription desc,
                  const ComPtr<ID3D12CommandQueue>& commandQueue,
                  const PlatformWindow& platformWindow);
    virtual ~DX12SwapChain() override;
    virtual void AcquireNextBackbuffer(u8 frameIndex) override;
    virtual void Present() override;
    virtual void Resize(u32 width, u32 height) override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual UniqueHandle<RHITexture> CreateBackBuffer(u8 backBufferIndex) override;

private:
    static u8 GetBackBufferCount(FrameBuffering frameBuffering);

    SwapChainDescription description;
    ComPtr<IDXGISwapChain4> swapChain;
};

} // namespace vex::dx12