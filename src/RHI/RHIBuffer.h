#pragma once

#include <span>

#include <Vex/Buffer.h>
#include <Vex/Debug.h>
#include <Vex/RHIFwd.h>
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

struct RHIMappedBufferMemory
{
    virtual ~RHIMappedBufferMemory() = 0;
    virtual void SetData(std::span<const u8> inData) = 0;
};

template <class Derived>
class RHIBufferInterface
{
public:
    class MappedMemory
    {
    public:
        MappedMemory(Derived& buffer, bool useStagingBuffer)
            : buffer(buffer)
            , useStagingBuffer(useStagingBuffer)
        {
            data = buffer.Map();
        }

        ~MappedMemory()
        {
            buffer.Unmap();
            if (useStagingBuffer)
            {
                buffer.SetNeedsStagingBufferCopy(true);
            }
        }

        void SetData(std::span<const u8> inData)
        {
            VEX_ASSERT(data.size() >= inData.size());
            std::ranges::copy(inData, data.begin());
        }

    private:
        Derived& buffer;
        std::span<u8> data;
        bool useStagingBuffer;
    };

    // RAII safe version to access buffer memory
    MappedMemory GetMappedMemory()
    {
        if (ShouldUseStagingBuffer())
        {
            if (!stagingBuffer)
            {
                stagingBuffer = static_cast<Derived*>(this)->CreateStagingBuffer();
            }
            return MappedMemory(*static_cast<Derived*>(this), true);
        }
        return MappedMemory(*static_cast<Derived*>(this), false);
    }

    // Raw direct access to buffer memory
    virtual UniqueHandle<Derived> CreateStagingBuffer() = 0;
    virtual std::span<u8> Map() = 0;
    virtual void Unmap() = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;

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

    Derived* GetStagingBuffer()
    {
        VEX_ASSERT(stagingBuffer);
        return stagingBuffer.get();
    };

protected:
    explicit RHIBufferInterface(const BufferDescription& desc)
        : desc{ desc }
    {
    }

    bool ShouldUseStagingBuffer() const
    {
        // Any buffer which does not have CPUWrite requires a staging buffer for the upload of data.
        return !(desc.usage & BufferUsage::CPUWrite);
    }

    BufferDescription desc;
    RHIBufferState::Flags currentState = RHIBufferState::Common;

    UniqueHandle<Derived> stagingBuffer;
    bool needsStagingBufferCopy = false;
};

} // namespace vex