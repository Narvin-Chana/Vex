#pragma once

#include <array>

#include <Vex/RHIFwd.h>

#include <RHI/RHI.h>

#include <DX12/DX12FeatureChecker.h>

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::dx12
{

class DX12RHI final : public RenderHardwareInterface
{
public:
    DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    ~DX12RHI();

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) override;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() override;
    virtual UniqueHandle<RHIGraphicsPipelineState> CreateGraphicsPipelineState(
        const GraphicsPipelineStateKey& key) override;
    virtual UniqueHandle<RHIComputePipelineState> CreateComputePipelineState(
        const ComputePipelineStateKey& key) override;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) override;

    virtual UniqueHandle<RHITexture> CreateTexture(const TextureDescription& description) override;
    virtual UniqueHandle<RHIBuffer> CreateBuffer(const BufferDescription& description) override;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() override;

    virtual void ExecuteCommandList(RHICommandList& commandList) override;
    virtual void ExecuteCommandLists(std::span<RHICommandList*> commandLists) override;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) override;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;
    virtual void ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args,
                                                 std::vector<ShaderDefine>& defines) override;

private:
    ComPtr<ID3D12CommandQueue>& GetQueue(CommandQueueType queueType);

    bool enableGPUDebugLayer = false;

    ComPtr<DX12Device> device;

    std::array<ComPtr<ID3D12CommandQueue>, CommandQueueTypes::Count> queues;
};

} // namespace vex::dx12