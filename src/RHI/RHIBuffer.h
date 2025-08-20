#pragma once

#include <span>

#include <Vex/Buffer.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

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

class MappedMemory
{
public:
    MappedMemory(RHIBuffer& buffer, bool useStagingBuffer);
    ~MappedMemory();
    void SetData(std::span<const u8> inData);

private:
    RHIBuffer& buffer;
    std::span<u8> data;
    bool useStagingBuffer;
};

class RHIBufferBase
{
public:
    RHIBufferBase() = default;
    RHIBufferBase(const RHIBufferBase&) = delete;
    RHIBufferBase& operator=(const RHIBufferBase&) = delete;
    RHIBufferBase(RHIBufferBase&&) = default;
    RHIBufferBase& operator=(RHIBufferBase&&) = default;
    ~RHIBufferBase() = default;

    // RAII safe version to access buffer memory
    MappedMemory GetMappedMemory();

    // Raw direct access to buffer memory
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() = 0;
    virtual std::span<u8> Map() = 0;
    virtual void Unmap() = 0;
    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   u32 stride,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeAllocation(RHIAllocator& allocator) = 0;

    [[nodiscard]] bool NeedsStagingBufferCopy() const noexcept
    {
        return needsStagingBufferCopy;
    }

    void SetNeedsStagingBufferCopy(const bool value)
    {
        needsStagingBufferCopy = value;
    }

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

    RHIBuffer* GetStagingBuffer();

protected:
    explicit RHIBufferBase(const BufferDescription& desc);
    bool ShouldUseStagingBuffer() const;

    BufferDescription desc;
    RHIBufferState::Flags currentState = RHIBufferState::Common;

    UniqueHandle<RHIBuffer> stagingBuffer;
    bool needsStagingBufferCopy = false;
};

} // namespace vex