#include "RHIBuffer.h"

#include "Vex/Debug.h"

namespace vex
{

struct DirectBufferMemory : RHIMappedBufferMemory
{
    explicit DirectBufferMemory(RHIBuffer& target)
        : buffer{ target }
    {
        data = buffer.Map();
    }

    virtual void SetData(std::span<const u8> inData) override
    {
        VEX_ASSERT(data.size() >= inData.size());
        std::ranges::copy(inData, data.begin());
    }

    ~DirectBufferMemory() override
    {
        buffer.UnMap();
    }

    std::span<u8> data;
    RHIBuffer& buffer;
};

struct StagedBufferMemory : DirectBufferMemory
{
    explicit StagedBufferMemory(RHIBuffer& stagingBuffer, RHIBuffer& realBuffer)
        : DirectBufferMemory(stagingBuffer)
        , realBuffer{ realBuffer }
    {
    }

    ~StagedBufferMemory() override
    {
        realBuffer.SetNeedsStagingBufferCopy(true);
    }

    RHIBuffer& realBuffer;
};

bool DoesBufferNeedStagingBuffer(const BufferDescription& desc)
{
    return (desc.memoryAcces & BufferMemoryAccess::GPURead | desc.memoryAcces & BufferMemoryAccess::GPUWrite) &&
           !(desc.memoryAcces & BufferMemoryAccess::CPURead | desc.memoryAcces & BufferMemoryAccess::CPUWrite);
}

UniqueHandle<RHIMappedBufferMemory> RHIBuffer::GetMappedMemory()
{
    if (DoesBufferNeedStagingBuffer(desc))
    {
        if (!stagingBuffer)
        {
            stagingBuffer = CreateStagingBuffer();
        }
        return MakeUnique<StagedBufferMemory>(*stagingBuffer, *this);
    }
    return MakeUnique<DirectBufferMemory>(*this);
}

} // namespace vex
