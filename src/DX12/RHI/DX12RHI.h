#pragma once

#include <array>
#include <optional>

#include <Vex/RHIImpl/RHIFence.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHI.h>
#include <RHI/RHIFwd.h>

namespace vex
{
struct PlatformWindowHandle;
}

// Enable to output live objects upon RHI deletion.
#ifndef VEX_DX12RHI_REPORT_LIVE_OBJECTS
#define VEX_DX12RHI_REPORT_LIVE_OBJECTS 0
#endif

namespace vex::dx12
{
class DX12RHI final : public RHIBase
{
public:
    DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    ~DX12RHI();

    static std::vector<UniqueHandle<RHIPhysicalDevice>> EnumeratePhysicalDevices();

    virtual void Init() override;

    virtual RHISwapChain CreateSwapChain(SwapChainDesc& desc, const PlatformWindow& platformWindow) override;

    virtual RHICommandPool CreateCommandPool() override;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) override;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) override;
    virtual RHIRayTracingPipelineState CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key) override;
    virtual RHIResourceLayout CreateResourceLayout(RHIDescriptorPool& descriptorPool) override;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDesc& desc) override;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDesc& desc) override;

    virtual RHIDescriptorPool CreateDescriptorPool() override;

    virtual RHIAllocator CreateAllocator() override;

    virtual RHITimestampQueryPool CreateTimestampQueryPool(RHIAllocator& allocator) override;

    virtual RHIAccelerationStructure CreateAS(const ASDesc& desc) override;

    virtual void WaitForTokenOnCPU(const SyncToken& syncToken) override;
    virtual bool IsTokenComplete(const SyncToken& syncToken) const override;
    virtual void WaitForTokenOnGPU(QueueType waitingQueue, const SyncToken& waitFor) override;

    virtual std::array<SyncToken, QueueTypes::Count> GetMostRecentSyncTokenPerQueue() const override;

    virtual std::vector<SyncToken> Submit(Span<const NonNullPtr<RHICommandList>> commandLists,
                                          Span<const SyncToken> dependencies) override;

    virtual void FlushGPU() override;

    ComPtr<DX12Device>& GetNativeDevice();
    ComPtr<ID3D12CommandQueue>& GetNativeQueue(QueueType queueType);

private:
    bool enableGPUDebugLayer = false;

    struct LiveObjectsReporter
    {
        ~LiveObjectsReporter();
    };
    std::optional<LiveObjectsReporter> liveObjectsReporter;

    ComPtr<DX12Device> device;

    std::array<ComPtr<ID3D12CommandQueue>, QueueTypes::Count> queues;
    std::optional<std::array<DX12Fence, QueueTypes::Count>> fences;

    friend class DX12SwapChain;
};

} // namespace vex::dx12