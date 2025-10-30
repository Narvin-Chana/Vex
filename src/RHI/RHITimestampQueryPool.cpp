#include "RHITimestampQueryPool.h"

#include <Vex/GfxBackend.h>
#include <Vex/Validation.h>

namespace vex
{

RHITimestampQueryPoolBase::RHITimestampQueryPoolBase(GfxBackend& graphics)
    : graphics{ graphics }
{
    timestampBuffer = graphics.CreateBuffer({ .name = "TimestampQueryReadback",
                                              .usage = BufferUsage::GenericBuffer,
                                              .byteSize = sizeof(u64) * MaxQuerySlotsAtOnce,
                                              .memoryLocality = ResourceMemoryLocality::CPURead });
}

u64 RHITimestampQueryPoolBase::GetTimestampInterval(QueryHandle handle)
{
    VEX_CHECK(inFlightQueries.IsValid(handle), "QueryHandle needs to be valid to retrieve timestamp data");

    MaybeUninitialized<Query> query = inFlightQueries.ExtractElement(handle);
    u64 timestampBefore, timestampAfter;

    VEX_CHECK(query->ready,
              "Query needs to be ready to be read. This should be the case once the command list is closed")
    {
        ResourceMappedMemory mem = graphics->MapResource(timestampBuffer);
        mem.ReadData(query->startIndex, std::as_writable_bytes(std::span{ &timestampBefore, 1 }));
        mem.ReadData(query->endIndex, std::as_writable_bytes(std::span{ &timestampAfter, 1 }));
    }

    return timestampAfter - timestampBefore;
}
} // namespace vex