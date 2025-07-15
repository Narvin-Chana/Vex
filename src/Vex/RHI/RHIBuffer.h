#pragma once

#include <span>

#include <Vex/Buffer.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

BEGIN_VEX_ENUM_FLAGS(RHIBufferState, u8)
    Common,
    CopySource,
    CopyDest,
    ConstantBuffer,
    StructuredBuffer,
    IndexBuffer,
    VertexBuffer,
    ShaderRead,
END_VEX_ENUM_FLAGS();

// clang-format on

class RHIBuffer
{
protected:
    BufferDescription desc;

    RHIBuffer(const BufferDescription& desc)
        : desc{ desc }
    {
    }

    RHIBufferState::Flags currentState = RHIBufferState::Common;

public:
    virtual bool CanBeMapped() = 0;
    virtual std::span<u8> Map() = 0;
    virtual void UnMap() = 0;

    void SetCurrentState(const RHIBufferState::Flags flags)
    {
        currentState = flags;
    }
    RHIBufferState::Flags GetCurrentState() const noexcept
    {
        return currentState;
    }

    const BufferDescription& GetDescription() const noexcept
    {
        return desc;
    };

    virtual ~RHIBuffer() = default;
};

} // namespace vex