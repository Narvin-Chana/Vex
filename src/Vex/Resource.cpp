#include "Resource.h"

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>

namespace vex
{

ResourceMappedMemory::ResourceMappedMemory(IMappableResource& resource, const std::function<void()>& inDeferredAction)
    : ResourceMappedMemory(resource)
{
    deferredAction = inDeferredAction;
}

ResourceMappedMemory::ResourceMappedMemory(ResourceMappedMemory&& other) noexcept
    : mappedData{ std::move(other.mappedData) }
    , resource{ other.resource }
    , mapped{ std::exchange(other.mapped, false) }
    , deferredAction{ std::move(other.deferredAction) }
{
}

ResourceMappedMemory& ResourceMappedMemory::operator=(ResourceMappedMemory&& other) noexcept
{
    resource = other.resource;
    mappedData = std::move(other.mappedData);
    deferredAction = std::move(other.deferredAction);
    mapped = std::exchange(other.mapped, false);
    return *this;
}

ResourceMappedMemory::ResourceMappedMemory(IMappableResource& resource)
    : resource{ resource }
    , mapped{ true }
{
    mappedData = resource.Map();
}

ResourceMappedMemory::ResourceMappedMemory(IMappableResource& resource,
                                           std::move_only_function<void()>&& inDeferredAction)
    : ResourceMappedMemory(resource)
{
    deferredAction = std::move(inDeferredAction);
}

ResourceMappedMemory::~ResourceMappedMemory()
{
    if (mapped)
    {
        resource.Unmap();
        deferredAction();
    }
}

void ResourceMappedMemory::SetData(std::span<const u8> inData)
{
    VEX_ASSERT(mappedData.size() >= inData.size());
    // This function is defined in <utility> which is more lightweight than <ranges>/<algorithm>.
    std::ranges::copy(inData, mappedData.begin());
}

void ResourceMappedMemory::SetData(std::span<u8> inData)
{
    SetData(std::span<const u8>{ inData.begin(), inData.end() });
}

} // namespace vex