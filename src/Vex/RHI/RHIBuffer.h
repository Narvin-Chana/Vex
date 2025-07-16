#pragma once

#include "Vex/UniqueHandle.h"

#include <span>

#include <Vex/Buffer.h>
#include <Vex/Types.h>

namespace vex
{
class RHIDescriptorPool;

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

// RAII structure to wrap Map and Unmap operations on a buffer
class RHIMappedBufferMemory
{
public:
    virtual ~RHIMappedBufferMemory() = default;
    virtual void SetData(std::span<const u8> data) = 0;
};

class RHIBuffer
{
protected:
    BufferDescription desc;

    RHIBuffer(const BufferDescription& desc)
        : desc{ desc }
    {
    }

    RHIBufferState::Flags currentState = RHIBufferState::Common;
    bool needsStagingBufferCopy = false;

public:
    virtual UniqueHandle<RHIMappedBufferMemory> GetMappedMemory() = 0;

    bool NeedsStagingBufferCopy() const noexcept
    {
        return needsStagingBufferCopy;
    };
    void SetNeedsStagingBufferCopy(const bool value)
    {
        needsStagingBufferCopy = value;
    }

    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;

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

    virtual RHIBuffer* GetStagingBuffer() = 0;
    virtual ~RHIBuffer() = default;
};

} // namespace vex