#pragma once

#include <array>
#include <span>
#include <utility>
#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/RHIFwd.h>
#include <Vex/ResourceCopy.h>
#include <Vex/Synchronization.h>
#include <Vex/Types.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{
struct RHIDrawResources;
struct DrawResources;
struct ConstantBinding;
struct ResourceBinding;
struct RHITextureBinding;
struct RHIBufferBinding;
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
    RHICommandListBase(CommandQueueType type)
        : type(type)
    {
    }

    virtual void Open() = 0;
    virtual void Close() = 0;

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

    virtual void Transition(RHITexture& texture, RHITextureState newState) = 0;
    virtual void Transition(RHIBuffer& texture, RHIBufferState::Flags newState) = 0;
    // Ideal for batching multiple resource transitions together.
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState>> textureNewStatePairs) = 0;
    virtual void Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs) = 0;

    // Need to be called before and after all draw commands with the same DrawBinding
    virtual void BeginRendering(const RHIDrawResources& resources) = 0;
    virtual void EndRendering() = 0;

    virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 vertexOffset = 0, u32 instanceOffset = 0) = 0;
    virtual void DrawIndexed(
        u32 indexCount, u32 instanceCount = 1, u32 indexOffset = 0, u32 vertexOffset = 0, u32 instanceOffset = 0) = 0;

    virtual void SetVertexBuffers(u32 startSlot, std::span<RHIBufferBinding> vertexBuffers) = 0;
    virtual void SetIndexBuffer(const RHIBufferBinding& indexBuffer) = 0;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) = 0;

    virtual void TraceRays(const std::array<u32, 3>& widthHeightDepth,
                           const RHIRayTracingPipelineState& rayTracingPipelineState) = 0;

    // Copies the whole texture data from src to dst. These textures should have the same size, mips, slice, type,
    // etc...
    virtual void Copy(RHITexture& src, RHITexture& dst);
    // Copies from src to dst the various copy descriptions.
    virtual void Copy(RHITexture& src,
                      RHITexture& dst,
                      std::span<const TextureCopyDescription> textureCopyDescriptions) = 0;
    // Copies the whole contents of src to dst. Buffers need to have the same byte size.
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst);
    // Copies the buffer from src to dst.
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDescription& bufferCopyDescription) = 0;
    // Copies the complete contents of the buffer to all regions of the dst texture.
    // This assumes the texture regions are contiguously arranged in the buffer. The whole content of the texture must
    // be in the buffer.
    virtual void Copy(RHIBuffer& src, RHITexture& dst);
    // Copies the different regions of buffers to dst texture regions.
    virtual void Copy(RHIBuffer& src,
                      RHITexture& dst,
                      std::span<const BufferToTextureCopyDescription> bufferToTextureCopyDescriptions) = 0;

    CommandQueueType GetType() const
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

    std::span<const SyncToken> GetSyncTokens() const
    {
        return syncTokens;
    }
    void SetSyncTokens(std::span<SyncToken> tokens)
    {
        syncTokens = { tokens.begin(), tokens.end() };
    }

    bool IsOpen() const
    {
        return isOpen;
    }

protected:
    CommandQueueType type;

    RHICommandListState state = RHICommandListState::Available;
    std::vector<SyncToken> syncTokens;

    bool isOpen = false;
};

} // namespace vex