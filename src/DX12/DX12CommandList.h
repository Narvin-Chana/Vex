#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHICommandList.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12CommandList : public RHICommandList
{
public:
    DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type);
    virtual ~DX12CommandList() = default;

    virtual bool IsOpen() const override;

    virtual void Open() override;
    virtual void Close() override;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) override;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) override;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) override;

    virtual void SetLayout(RHIResourceLayout& layout) override;
    virtual void SetLayoutLocalConstants(const RHIResourceLayout& layout,
                                         std::span<const ConstantBinding> constants) override;
    virtual void SetLayoutResources(const RHIResourceLayout& layout,
                                    std::span<RHITextureBinding> textures,
                                    std::span<RHIBufferBinding> buffers,
                                    RHIDescriptorPool& descriptorPool) override;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) override;

    virtual void Transition(RHITexture& texture, RHITextureState::Flags newState) override;
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) override;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) override;

    virtual void Copy(RHITexture& src, RHITexture& dst) override;

    virtual CommandQueueType GetType() const override;

private:
    ComPtr<DX12Device> device;
    CommandQueueType type;
    ComPtr<ID3D12GraphicsCommandList10> commandList;

    // Underlying memory of the command list.
    ComPtr<ID3D12CommandAllocator> commandAllocator;

    bool isOpen = false;

    friend class DX12RHI;
};

} // namespace vex::dx12