#pragma once

#include <array>
#include <optional>

#include <Vex/RHIFwd.h>

#include <RHI/RHI.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/DX12Fence.h>

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::dx12
{

class DX12RHI final : public RHIBase
{
public:
    DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    ~DX12RHI();

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) override;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() override;
    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) override;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) override;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) override;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDescription& description) override;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDescription& description) override;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() override;

    virtual RHIAllocator CreateAllocator() override;

    virtual void ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args,
                                                 std::vector<ShaderDefine>& defines) override;

    virtual u32 AcquireNextFrame(RHISwapChain& swapChain, u32 currentFrameIndex) override;
    virtual void SubmitAndPresent(std::span<RHICommandList*> commandLists,
                                  RHISwapChain& swapChain,
                                  u32 currentFrameIndex,
                                  bool isFullscreenMode) override;
    virtual void FlushGPU() override;

    ComPtr<DX12Device>& GetNativeDevice();
    ComPtr<ID3D12CommandQueue>& GetNativeQueue(CommandQueueType queueType);

private:
    bool enableGPUDebugLayer = false;

    struct LiveObjectsReporter
    {
        ~LiveObjectsReporter();
    };
    std::optional<LiveObjectsReporter> liveObjectsReporter;

    ComPtr<DX12Device> device;

    std::array<ComPtr<ID3D12CommandQueue>, CommandQueueTypes::Count> queues;
    std::optional<std::array<DX12Fence, CommandQueueTypes::Count>> fences;
};

} // namespace vex::dx12