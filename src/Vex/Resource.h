#pragma once

#include <span>

#include <Vex/Concepts.h>
#include <Vex/Handle.h>
#include <Vex/Types.h>

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

struct BindlessHandle : Handle<BindlessHandle>
{
};

static constexpr BindlessHandle GInvalidBindlessHandle;

struct MappableResourceInterface
{
    virtual std::span<byte> Map() = 0;
    virtual void Unmap() = 0;
    virtual ~MappableResourceInterface() = default;
};

class ResourceMappedMemory
{
public:
    ResourceMappedMemory() = delete;
    ResourceMappedMemory(const ResourceMappedMemory&) = delete;
    ResourceMappedMemory& operator=(const ResourceMappedMemory&) = delete;

    ResourceMappedMemory(ResourceMappedMemory&&) noexcept;
    ResourceMappedMemory& operator=(ResourceMappedMemory&&) noexcept;

    ResourceMappedMemory(MappableResourceInterface& resource);

    ~ResourceMappedMemory();
    void WriteData(std::span<const byte> inData);
    void WriteData(std::span<byte> inData);
    void WriteData(std::span<const byte> inData, u32 offset);
    void WriteData(std::span<byte> inData, u32 offset);
    void ReadData(u32 offset, std::span<byte> outData);
    void ReadData(std::span<byte> outData);

    template <class T>
        requires(not IsContainer<T>)
    void WriteData(const T& inData);
    template <class T>
    void ReadData(T& outData);

    [[nodiscard]] std::span<byte> GetMappedRange() const
    {
        return mappedData;
    };

private:
    std::span<byte> mappedData;

    MappableResourceInterface& resource;

    bool isMapped = false;
};

template <class T>
    requires(not IsContainer<T>)
void ResourceMappedMemory::WriteData(const T& inData)
{
    const byte* dataPtr = reinterpret_cast<const byte*>(&inData);
    WriteData(std::span{ dataPtr, sizeof(T) });
}

template <class T>
void ResourceMappedMemory::ReadData(T& outData)
{
    byte* dataPtr = reinterpret_cast<byte*>(&outData);
    ReadData(std::as_bytes(std::span{ dataPtr, sizeof(T) }));
}

} // namespace vex