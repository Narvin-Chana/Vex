#pragma once

#include <print>
#include <span>

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
    virtual std::span<u8> Map() = 0;
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
    void SetData(std::span<const u8> inData);
    void SetData(std::span<u8> inData);

    template <class T>
    void SetData(const T& inData);

private:
    std::span<u8> mappedData;

    MappableResourceInterface& resource;

    bool isMapped = false;
};

template <class T>
void ResourceMappedMemory::SetData(const T& inData)
{
    const u8* dataPtr = reinterpret_cast<const u8*>(&inData);
    SetData(std::span{ dataPtr, sizeof(T) });
}

} // namespace vex