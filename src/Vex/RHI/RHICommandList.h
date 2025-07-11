#pragma once


#include <array>
#include <span>
#include <utility>

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHITexture.h>
#include <Vex/Types.h>

namespace vex
{
struct ConstantBinding;
struct ResourceBinding;
class RHITexture;
class RHIBuffer;
struct RHITextureBinding;
struct RHIBufferBinding;

class RHICommandList
{
public:
    virtual ~RHICommandList() = default;

    virtual bool IsOpen() const = 0;

    virtual void Open() = 0;
    virtual void Close() = 0;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) = 0;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) = 0;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) = 0;

    virtual void SetLayout(RHIResourceLayout& layout) = 0;
    virtual void SetLayoutLocalConstants(const RHIResourceLayout& layout,
                                         std::span<const ConstantBinding> constants) = 0;
    virtual void SetLayoutResources(const RHIResourceLayout& layout,
                                    std::span<RHITextureBinding> textures,
                                    std::span<RHIBufferBinding> buffers,
                                    RHIDescriptorPool& descriptorPool) = 0;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) = 0;

    virtual void Transition(RHITexture& texture, RHITextureState::Flags newState) = 0;
    // Ideal for batching multiple resource transitions together.
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) = 0;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) = 0;

    virtual void Copy(RHITexture& src, RHITexture& dst) = 0;

    // TODO: implement (not using VEX_NOT_YET_IMPLEMENTED since we're in a .h), will be done in a Buffer-specific PR.
    // virtual void Copy(RHIBuffer& src, RHIBuffer& dst)
    //{
    //}

    virtual CommandQueueType GetType() const = 0;
};

} // namespace vex