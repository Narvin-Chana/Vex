#pragma once

#include <span>

#include <Vex/RHI/RHI.h>
#include <Vex/Types.h>

namespace vex
{
struct ConstantBinding;
class RHITexture;
class RHIBuffer;

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

    virtual void Dispatch(const std::array<u32, 3>& groupCount, RHIResourceLayout& layout, RHITexture& backbuffer) = 0;

    virtual CommandQueueType GetType() const = 0;
};

} // namespace vex