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
    VEX_ASSERT(byteOffset <= mappedData.size() && inData.size() <= mappedData.size() - byteOffset);
    std::ranges::copy(inData, mappedData.begin() + byteOffset);
}

void MappedMemory::ReadData(u64 byteOffset, Span<byte> outData)
{
    VEX_ASSERT(byteOffset <= mappedData.size() && outData.size() <= mappedData.size() - byteOffset);
    std::copy_n(mappedData.begin() + byteOffset, outData.size(), outData.begin());
}

void MappedMemory::ReadData(Span<byte> outData)
{
    ReadData(0, outData);
}

} // namespace vex