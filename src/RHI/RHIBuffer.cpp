#include "RHIBuffer.h"

#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex
{

MappedMemory::MappedMemory(RHIBuffer& buffer, bool useStagingBuffer)
    : buffer(buffer)
    , useStagingBuffer(useStagingBuffer)
{
    data = useStagingBuffer ? buffer.GetStagingBuffer()->Map() : buffer.Map();
}

MappedMemory::~MappedMemory()
{
    if (!useStagingBuffer)
    {
        buffer.Unmap();
    }
    else
    {
        buffer.GetStagingBuffer()->Unmap();
        buffer.SetNeedsStagingBufferCopy(true);
    }
}

void MappedMemory::SetData(std::span<const u8> inData)
{
    VEX_ASSERT(data.size() >= inData.size());
    std::ranges::copy(inData, data.begin());
}

MappedMemory RHIBufferInterface::GetMappedMemory()
{
    if (ShouldUseStagingBuffer())
    {
        if (!stagingBuffer)
        {
            stagingBuffer = CreateStagingBuffer();
        }
        return MappedMemory(*static_cast<RHIBuffer*>(this), true);
    }
    return MappedMemory(*static_cast<RHIBuffer*>(this), false);
}

RHIBufferInterface::RHIBufferInterface(const BufferDescription& desc)
    : desc{ desc }
{
}

bool RHIBufferInterface::ShouldUseStagingBuffer() const
{
    // Any buffer which does not have CPUWrite requires a staging buffer for the upload of data.
    return desc.memoryLocality != ResourceMemoryLocality::CPUWrite;
}

RHIBuffer* RHIBufferInterface::GetStagingBuffer()
{
    VEX_ASSERT(stagingBuffer);
    return stagingBuffer.get();
}

} // namespace vex