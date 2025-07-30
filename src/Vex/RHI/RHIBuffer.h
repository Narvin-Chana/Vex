#pragma once

#include <span>

#include <Vex/Buffer.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{
class RHIDescriptorPool;

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

// RAII structure to wrap Map and Unmap operations on a buffer
class RHIMappedBufferMemory
{
public:
    virtual ~RHIMappedBufferMemory() = default;
    virtual void SetData(std::span<const u8> data) = 0;
};

class RHIBuffer
{
public:
    // RAII safe version to access buffer memory
    UniqueHandle<RHIMappedBufferMemory> GetMappedMemory();

    // Raw direct access to buffer memory
    virtual std::span<u8> Map() = 0;
    virtual void Unmap() = 0;

    [[nodiscard]] bool NeedsStagingBufferCopy() const noexcept
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

    [[nodiscard]] RHIBufferState::Flags GetCurrentState() const noexcept
    {
        return currentState;
    }

    [[nodiscard]] const BufferDescription& GetDescription() const noexcept
    {
        return desc;
    };

    virtual RHIBuffer* GetStagingBuffer()
    {
        return stagingBuffer.get();
    };
    virtual ~RHIBuffer() = default;

protected:
    BufferDescription desc;

    explicit RHIBuffer(const BufferDescription& desc)
        : desc{ desc }
    {
    }

    UniqueHandle<RHIBuffer> stagingBuffer;
    RHIBufferState::Flags currentState = RHIBufferState::Common;
    bool needsStagingBufferCopy = false;

    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() = 0;
};

} // namespace vex