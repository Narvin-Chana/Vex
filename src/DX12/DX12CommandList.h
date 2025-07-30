#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHICommandList.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12CommandList final
{
public:
    DX12CommandList(const ComPtr<DX12Device>& device, CommandQueueType type);

    bool IsOpen() const;

    void Open();
    void Close();

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(i32 x, i32 y, u32 width, u32 height);

    void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState);
    void SetPipelineState(const RHIComputePipelineState& computePipelineState);

    void SetLayout(RHIResourceLayout& layout);
    void SetLayoutLocalConstants(const RHIResourceLayout& layout, std::span<const ConstantBinding> constants);
    void SetLayoutResources(const RHIResourceLayout& layout,
                            std::span<RHITextureBinding> textures,
                            std::span<RHIBufferBinding> buffers,
                            RHIDescriptorPool& descriptorPool);
    void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout);
    void SetInputAssembly(InputAssembly inputAssembly);

    void ClearTexture(RHITexture& rhiTexture, const ResourceBinding& clearBinding, const TextureClearValue& clearValue);

    void Transition(RHITexture& texture, RHITextureState::Flags newState);
    void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs);

    void Draw(u32 vertexCount);

    void Dispatch(const std::array<u32, 3>& groupCount);

    void Copy(RHITexture& src, RHITexture& dst);

    CommandQueueType GetType() const;

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