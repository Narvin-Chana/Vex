#pragma once

#include <array>
#include <span>
#include <utility>

#include <Vex/CommandQueueType.h>
#include <Vex/RHIFwd.h>
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

class RHICommandListBase
{
public:
    virtual bool IsOpen() const = 0;

    virtual void Open() = 0;
    virtual void Close() = 0;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) = 0;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) = 0;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) = 0;

    virtual void SetLayout(RHIResourceLayout& layout, RHIDescriptorPool& descriptorPool) = 0;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) = 0;
    virtual void SetInputAssembly(InputAssembly inputAssembly) = 0;

    virtual void ClearTexture(RHITexture& rhiTexture,
                              const ResourceBinding& clearBinding,
                              const TextureClearValue& clearValue) = 0;

    virtual void Transition(RHITexture& texture, RHITextureState::Flags newState) = 0;
    virtual void Transition(RHIBuffer& texture, RHIBufferState::Flags newState) = 0;
    // Ideal for batching multiple resource transitions together.
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) = 0;
    virtual void Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs) = 0;

    // Need to be called before and after all draw commands with the same DrawBinding
    virtual void BeginRendering(const RHIDrawResources& resources) = 0;
    virtual void EndRendering() = 0;

    virtual void Draw(u32 vertexCount) = 0;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) = 0;

    virtual void Copy(RHITexture& src, RHITexture& dst) = 0;
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst) = 0;

    virtual CommandQueueType GetType() const = 0;
};

} // namespace vex