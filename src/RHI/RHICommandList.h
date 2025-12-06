#pragma once

#include <array>
#include <Vex/Containers/Span.h>
#include <utility>
#include <vector>

#include <Vex/QueueType.h>
#include <Vex/ResourceCopy.h>
#include <Vex/Synchronization.h>
#include <Vex/Types.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHIFwd.h>
#include <RHI/RHITexture.h>
#include <RHI/RHITimestampQueryPool.h>

namespace vex
{
struct RHIDrawResources;
struct RHIBufferBinding;
struct RHITextureBinding;
struct InputAssembly;

enum class RHICommandListState : u8
{
    Available, // Ready to be acquired.
    Recording, // Currently being recorded to.
    Submitted  // Submitted to GPU, waiting for completion.
};

class RHICommandListBase
{
public:
    RHICommandListBase(QueueType type)
        : type(type)
    {
    }

    virtual void Open();
    virtual void Close();

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) = 0;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) = 0;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) = 0;
    virtual void SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState) = 0;

    virtual void SetLayout(RHIResourceLayout& layout) = 0;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) = 0;
    virtual void SetInputAssembly(InputAssembly inputAssembly) = 0;

    virtual void ClearTexture(const RHITextureBinding& binding,
                              TextureUsage::Type usage,
                              const TextureClearValue& clearValue) = 0;

    void BufferBarrier(RHIBuffer& buffer, RHIBarrierSync sync, RHIBarrierAccess access);
    void TextureBarrier(RHITexture& texture, RHIBarrierSync sync, RHIBarrierAccess access, RHITextureLayout layout);
    virtual void Barrier(Span<const RHIBufferBarrier> bufferBarriers,
                         Span<const RHITextureBarrier> textureBarriers) = 0;

    // Need to be called before and after all draw commands with the same DrawBinding
    virtual void BeginRendering(const RHIDrawResources& resources) = 0;
    virtual void EndRendering() = 0;

    virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 vertexOffset = 0, u32 instanceOffset = 0) = 0;
    virtual void DrawIndexed(
        u32 indexCount, u32 instanceCount = 1, u32 indexOffset = 0, u32 vertexOffset = 0, u32 instanceOffset = 0) = 0;

    virtual void SetVertexBuffers(u32 startSlot, Span<const RHIBufferBinding> vertexBuffers) = 0;
    virtual void SetIndexBuffer(const RHIBufferBinding& indexBuffer) = 0;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) = 0;

    virtual void TraceRays(const std::array<u32, 3>& widthHeightDepth,
                           const RHIRayTracingPipelineState& rayTracingPipelineState) = 0;

    virtual void GenerateMips(RHITexture& texture, const TextureSubresource& subresource) = 0;

    virtual QueryHandle BeginTimestampQuery() = 0;
    virtual void EndTimestampQuery(QueryHandle handle) = 0;
    virtual void ResolveTimestampQueries(u32 firstQuery, u32 queryCount) = 0;

    // Copies the whole texture data from src to dst. These textures should have the same size, mips, slice, type,
    // format, etc...
    virtual void Copy(RHITexture& src, RHITexture& dst);
    // Copies from src to dst the various copy descriptions.
    virtual void Copy(RHITexture& src, RHITexture& dst, Span<const TextureCopyDesc> textureCopyDescriptions) = 0;
    // Copies the whole contents of src to dst. Buffers need to have the same byte size.
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst);
    // Copies the buffer from src to dst.
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDescription) = 0;
    // Copies the complete contents of the buffer to the dst texture.
    // This assumes the texture regions are contiguously arranged in the buffer. The whole content of the texture must
    // be in the buffer.
    virtual void Copy(RHIBuffer& src, RHITexture& dst);
    // Copies the different regions of buffers to dst texture regions.
    virtual void Copy(RHIBuffer& src, RHITexture& dst, Span<const BufferTextureCopyDesc> copyDescs) = 0;

    // Copies a texture into a buffer.
    virtual void Copy(RHITexture& src, RHIBuffer& dst);
    // Copies a texture into a buffer using copyDescs.
    virtual void Copy(RHITexture& src, RHIBuffer& dst, Span<const BufferTextureCopyDesc> copyDescs) = 0;

    virtual RHIScopedGPUEvent CreateScopedMarker(const char* label, std::array<float, 3> labelColor) = 0;

    QueueType GetType() const
    {
        return type;
    }

    RHICommandListState GetState() const
    {
        return state;
    }
    void SetState(RHICommandListState newState)
    {
        state = newState;
    }

    Span<const SyncToken> GetSyncTokens() const
    {
        return syncTokens;
    }
    void SetSyncTokens(Span<const SyncToken> tokens);

    bool IsOpen() const
    {
        return isOpen;
    }

    void UpdateTimestampQueryTokens(SyncToken token);

    void SetTimestampQueryPool(NonNullPtr<RHITimestampQueryPool> inQueryPool)
    {
        queryPool = inQueryPool;
    }

protected:
    QueueType type;

    RHICommandListState state = RHICommandListState::Available;
    std::vector<SyncToken> syncTokens;
    SyncToken submissionToken;

    RHITimestampQueryPool* queryPool = nullptr;
    std::vector<QueryHandle> queries;

    bool isOpen = false;
};

} // namespace vex