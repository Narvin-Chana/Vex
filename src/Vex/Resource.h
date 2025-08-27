#pragma once

#include <functional>
#include <print>
#include <span>
#include <variant>

#include <Vex/Handle.h>
#include <Vex/RHIFwd.h>
#include <Vex/Types.h>

#include "RHI/RHIAllocator.h"

#define VEX_USE_CUSTOM_ALLOCATOR_BUFFERS 1

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

struct IMappableResource
{
    virtual std::span<u8> Map() = 0;
    virtual void Unmap() = 0;
    virtual ~IMappableResource() = default;
};

class ResourceMappedMemory
{
public:
    ResourceMappedMemory() = delete;
    ResourceMappedMemory(const ResourceMappedMemory&) = delete;
    ResourceMappedMemory& operator=(const ResourceMappedMemory&) = delete;

    ResourceMappedMemory(ResourceMappedMemory&&) noexcept;
    ResourceMappedMemory& operator=(ResourceMappedMemory&&) noexcept;

    ResourceMappedMemory(IMappableResource& resource);
    ResourceMappedMemory(IMappableResource& resource, const std::function<void()>& deferredAction);
    ResourceMappedMemory(IMappableResource& resource, std::move_only_function<void()>&& deferredAction);

    ~ResourceMappedMemory();
    void SetData(std::span<const u8> inData);
    void SetData(std::span<u8> inData);

    template <class T>
    void SetData(const T& inData);

private:
    std::span<u8> mappedData;

    IMappableResource& resource;

    bool mapped = false;

    std::move_only_function<void()> deferredAction = [] {};
};

template <class T>
void ResourceMappedMemory::SetData(const T& inData)
{
    const u8* dataPtr = reinterpret_cast<const u8*>(&inData);
    SetData(std::span{ dataPtr, sizeof(T) });
}

} // namespace vex