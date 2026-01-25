#pragma once

#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#include <Vex/Utility/Concepts.h>
#include <Vex/Utility/Handle.h>

#include <RHI/RHIFwd.h>

#ifndef VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
#define VEX_USE_CUSTOM_ALLOCATOR_BUFFERS 1
#endif

namespace vex
{

class CommandContext;

enum class ResourceLifetime : u8
{
    Static,  // Lives for many frames.
    Dynamic, // Is valid only for the current frame.
};

enum class ResourceMemoryLocality : u8
{
    GPUOnly,
    CPURead,
    CPUWrite,
};

struct BindlessHandle : Handle32<BindlessHandle>
{
};

static constexpr BindlessHandle GInvalidBindlessHandle;

class MappedMemory
{
public:
    MappedMemory(RHIBuffer& buffer);

    MappedMemory() = delete;
    MappedMemory(const MappedMemory&) = delete;
    MappedMemory& operator=(const MappedMemory&) = delete;

    MappedMemory(MappedMemory&&) = default;
    MappedMemory& operator=(MappedMemory&&) = default;
    ~MappedMemory() = default;

    void WriteData(Span<const byte> inData);
    void WriteData(Span<const byte> inData, u32 offset);
    void ReadData(u32 offset, Span<byte> outData);
    void ReadData(Span<byte> outData);

    template <class T>
        requires(not IsContainer<T>)
    void WriteData(const T& inData);
    template <class T>
        requires(not IsContainer<T>)
    void ReadData(T& outData);

    [[nodiscard]] Span<byte> GetMappedRange()
    {
        return mappedData;
    };

    [[nodiscard]] Span<const byte> GetMappedRange() const
    {
        return mappedData;
    };

private:
    Span<byte> mappedData;
};

template <class T>
    requires(not IsContainer<T>)
void MappedMemory::WriteData(const T& inData)
{
    const byte* dataPtr = reinterpret_cast<const byte*>(&inData);
    WriteData({ dataPtr, sizeof(T) });
}

template <class T>
    requires(not IsContainer<T>)
void MappedMemory::ReadData(T& outData)
{
    byte* dataPtr = reinterpret_cast<byte*>(&outData);
    ReadData({ dataPtr, sizeof(T) });
}

} // namespace vex