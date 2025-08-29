#include "Resource.h"

#include <utility>

#include <Vex/Debug.h>

namespace vex
{

ResourceMappedMemory::ResourceMappedMemory(ResourceMappedMemory&& other) noexcept
    : mappedData{ other.mappedData }
    , resource{ other.resource }
    , isMapped{ std::exchange(other.isMapped, false) }
{
}

ResourceMappedMemory& ResourceMappedMemory::operator=(ResourceMappedMemory&& other) noexcept
{
    resource = other.resource;
    mappedData = other.mappedData;
    isMapped = std::exchange(other.isMapped, false);
    return *this;
}

ResourceMappedMemory::ResourceMappedMemory(MappableResourceInterface& resource)
    : resource{ resource }
    , isMapped{ true }
{
    mappedData = resource.Map();
}

ResourceMappedMemory::~ResourceMappedMemory()
{
    if (isMapped)
    {
        resource.Unmap();
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