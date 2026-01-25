#include "Resource.h"

#include <utility>

#include <Vex/Platform/Debug.h>
#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex
{

MappedMemory::MappedMemory(RHIBuffer& buffer)
    : mappedData{ buffer.GetMappedData() }
{
}

void MappedMemory::WriteData(Span<const byte> inData)
{
    WriteData(inData, 0);
}

void MappedMemory::WriteData(Span<const byte> inData, u32 offset)
{
    VEX_ASSERT(mappedData.size() - offset >= inData.size());
    std::copy(inData.begin(), inData.end(), mappedData.begin() + offset);
}

void MappedMemory::ReadData(u32 offset, Span<byte> outData)
{
    VEX_ASSERT(mappedData.size() - offset >= outData.size());
    std::copy(mappedData.begin() + offset, mappedData.end(), outData.begin() + offset);
}

void MappedMemory::ReadData(Span<byte> outData)
{
    ReadData(0, outData);
}

} // namespace vex