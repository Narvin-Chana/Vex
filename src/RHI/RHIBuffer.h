#pragma once

#include "RHIAllocator.h"

#include <Vex/Buffer.h>
#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

BEGIN_VEX_ENUM_FLAGS(RHIBufferState, u16)
    Common = 0,
    CopySource = 1 << 0,
    CopyDest = 1 << 1,
    // UniformResource state is not exposed to users, as Vex fully leverages bindless rendering, 
    // where constant buffers are (from what we've seen) unusable.
    // We still expose the state, in case we ever decide to use them in a bindful (bindless-less?) manner.
    UniformResource = 1 << 2,
    ShaderResource = 1 << 3,
    ShaderReadWrite = 1 << 4,
    VertexBuffer = 1 << 5,
    IndexBuffer = 1 << 6,
    IndirectArgs = 1 << 7,
    RaytracingAccelerationStructure = 1 << 8,
END_VEX_ENUM_FLAGS();

// clang-format on

class RHIBufferBase : public IMappableResource
{
public:
    RHIBufferBase(RHIAllocator& allocator);
    RHIBufferBase(const RHIBufferBase&) = delete;
    RHIBufferBase& operator=(const RHIBufferBase&) = delete;
    RHIBufferBase(RHIBufferBase&&) = default;
    RHIBufferBase& operator=(RHIBufferBase&&) = default;
    ~RHIBufferBase() = default;

    // Raw direct access to buffer memory
    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   u32 stride,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeAllocation(RHIAllocator& allocator) = 0;

    void SetCurrentState(const RHIBufferState::Flags flags)
    {
        currentState = flags;
    }

    [[nodiscard]] RHIBufferState::Flags GetCurrentState() const noexcept
    {
        return currentState;
    }

    [[nodiscard]] const BufferDescription& GetDescription() const noexcept
    {
        return desc;
    };

    const Allocation& GetAllocation() const noexcept
    {
        return allocation;
    }

protected:
    explicit RHIBufferBase(RHIAllocator& allocator, const BufferDescription& desc);

    BufferDescription desc;
    RHIBufferState::Flags currentState = RHIBufferState::Common;

    NonNullPtr<RHIAllocator> allocator;
    Allocation allocation;
};

} // namespace vex