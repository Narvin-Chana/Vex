#pragma once

#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#include <Vex/Utility/Concepts.h>
#include <Vex/Utility/Handle.h>

#define VEX_USE_CUSTOM_ALLOCATOR_BUFFERS 0
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

struct MappableResourceInterface
{
    virtual Span<byte> Map() = 0;
    virtual void Unmap() = 0;
    virtual ~MappableResourceInterface() = default;
};

class ResourceMappedMemory
{
public:
    ResourceMappedMemory() = delete;
    ResourceMappedMemory(const ResourceMappedMemory&) = delete;
    ResourceMappedMemory& operator=(const ResourceMappedMemory&) = delete;

    ResourceMappedMemory(ResourceMappedMemory&&);
    ResourceMappedMemory& operator=(ResourceMappedMemory&&);

    ResourceMappedMemory(MappableResourceInterface& resource);

    ~ResourceMappedMemory();
    void WriteData(Span<const byte> inData);
    void WriteData(Span<byte> inData);
    void WriteData(Span<const byte> inData, u32 offset);
    void WriteData(Span<byte> inData, u32 offset);
    void ReadData(u32 offset, Span<byte> outData);
    void ReadData(Span<byte> outData);

    template <class T>
        requires(not IsContainer<T>)
    void WriteData(const T& inData);
    template <class T>
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

    MappableResourceInterface& resource;

    bool isMapped = false;
};

template <class T>
    requires(not IsContainer<T>)
void ResourceMappedMemory::WriteData(const T& inData)
{
    const byte* dataPtr = reinterpret_cast<const byte*>(&inData);
    WriteData({ dataPtr, sizeof(T) });
}

template <class T>
void ResourceMappedMemory::ReadData(T& outData)
{
    byte* dataPtr = reinterpret_cast<byte*>(&outData);
    ReadData({ dataPtr, sizeof(T) });
}

} // namespace vex