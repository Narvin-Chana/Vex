#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/Types.h>

namespace vex
{
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

    virtual CommandQueueType GetType() const = 0;
};

} // namespace vex