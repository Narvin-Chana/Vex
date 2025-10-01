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

void ResourceMappedMemory::WriteData(std::span<const byte> inData)
{
    WriteData(inData, 0);
}

void ResourceMappedMemory::WriteData(std::span<byte> inData)
{
    WriteData(std::span<const byte>{ inData.begin(), inData.end() }, 0);
}

void ResourceMappedMemory::WriteData(std::span<const byte> inData, u32 offset)
{
    VEX_ASSERT(mappedData.size() - offset >= inData.size());
    std::copy(inData.begin(), inData.end(), mappedData.begin() + offset);
}

void ResourceMappedMemory::WriteData(std::span<byte> inData, u32 offset)
{
    WriteData(std::span<const byte>{ inData.begin(), inData.end() }, offset);
}

void ResourceMappedMemory::ReadData(u32 offset, std::span<byte> outData)
{
    VEX_ASSERT(mappedData.size() - offset >= outData.size());
    std::copy(mappedData.begin() + offset, mappedData.end(), outData.begin() + offset);
}

void ResourceMappedMemory::ReadData(std::span<byte> outData)
{
    ReadData(0, outData);
}

} // namespace vex