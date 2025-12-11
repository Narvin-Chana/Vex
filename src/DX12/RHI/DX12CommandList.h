#pragma once

#include <RHI/RHI.h>
#include <RHI/RHICommandList.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12CommandList final : public RHICommandListBase
{
public:
    DX12CommandList(const ComPtr<DX12Device>& device, QueueType type);

    virtual void Open() override;
    virtual void Close() override;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) override;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) override;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) override;
    virtual void SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState) override;

    virtual void SetLayout(RHIResourceLayout& layout) override;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) override;
    virtual void SetInputAssembly(InputAssembly inputAssembly) override;

    virtual void ClearTexture(const RHITextureBinding& binding,
                              TextureUsage::Type usage,
                              const TextureClearValue& clearValue,
                              std::span<TextureClearRect> clearRects) override;

    virtual void Barrier(Span<const RHIBufferBarrier> bufferBarriers,
                         Span<const RHITextureBarrier> textureBarriers) override;

    virtual void BeginRendering(const RHIDrawResources& resources) override;
    virtual void EndRendering() override;

    virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 vertexOffset = 0, u32 instanceOffset = 0) override;
    virtual void DrawIndexed(
        u32 indexCount, u32 instanceCount, u32 indexOffset, u32 vertexOffset, u32 instanceOffset) override;

    virtual void SetVertexBuffers(u32 startSlot, Span<const RHIBufferBinding> vertexBuffers) override;
    virtual void SetIndexBuffer(const RHIBufferBinding& indexBuffer) override;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) override;

    virtual void TraceRays(const std::array<u32, 3>& widthHeightDepth,
                           const RHIRayTracingPipelineState& rayTracingPipelineState) override;

    virtual void GenerateMips(RHITexture& texture, const TextureSubresource& subresource) override;

    using RHICommandListBase::Copy;
    virtual void Copy(RHITexture& src, RHITexture& dst) override;
    virtual void Copy(RHITexture& src,
                      RHITexture& dst,
                      Span<const TextureCopyDesc> textureCopyDescriptions) override;
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDescription) override;
    virtual void Copy(RHIBuffer& src,
                      RHITexture& dst,
                      Span<const BufferTextureCopyDesc> copyDescriptions) override;
    virtual void Copy(RHITexture& src,
                      RHIBuffer& dst,
                      Span<const BufferTextureCopyDesc> copyDescriptions) override;

    virtual QueryHandle BeginTimestampQuery() override;
    virtual void EndTimestampQuery(QueryHandle handle) override;
    virtual void ResolveTimestampQueries(u32 firstQuery, u32 queryCount) override;

    virtual void BuildBLAS(RHIAccelerationStructure& as,
                           RHIBuffer& scratchBuffer,
                           const RHIBLASBuildDesc& desc) override;
    virtual void BuildTLAS(RHIAccelerationStructure& as, const RHITLASBuildDesc& desc) override;

    ComPtr<ID3D12GraphicsCommandList10>& GetNativeCommandList()
    {
        return commandList;
    }

    RHIScopedGPUEvent CreateScopedMarker(const char* label, std::array<float, 3> labelColor) override;

private:
    ComPtr<DX12Device> device;
    ComPtr<ID3D12GraphicsCommandList10> commandList;

    // Underlying memory of the command list.
    ComPtr<ID3D12CommandAllocator> commandAllocator;
};

} // namespace vex::dx12