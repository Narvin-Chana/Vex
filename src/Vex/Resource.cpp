#include "Resource.h"

#include <ranges>
#include <utility>

#include <Vex/RHIImpl/RHIBuffer.h>
#include <VexMacros.h>

namespace vex
{

MappedMemory::MappedMemory(const RHIBuffer& buffer)
    : mappedData{ buffer.GetMappedData() }
{
}

void MappedMemory::WriteData(Span<const byte> inData)
{
    WriteData(inData, 0);
}

void MappedMemory::WriteData(Span<const byte> inData, u64 byteOffset)
{
    VEX_ASSERT(mappedData.size() - byteOffset >= inData.size());
    std::ranges::copy(inData, mappedData.begin() + byteOffset);
}

void MappedMemory::ReadData(u64 byteOffset, Span<byte> outData)
{
    VEX_ASSERT(mappedData.size() - byteOffset >= outData.size());
    std::copy(mappedData.begin() + byteOffset, mappedData.end(), outData.begin() + byteOffset);
}

void MappedMemory::ReadData(Span<byte> outData)
{
    ReadData(0, outData);
}

} // namespace vex