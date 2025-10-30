#include "RHITimestampQueryPool.h"

#include <Vex/Graphics.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/Validation.h>

namespace vex
{

QueryHandle RHITimestampQueryPoolBase::AllocateQuery(QueueType queueType)
{
    u32 nextHead = (head + 1) % MaxInFlightQueriesCount;
    if (nextHead == tail)
    {
        u32 processedQueries = ResolveQueries();
        VEX_CHECK(processedQueries != 0,
                  "Unable to make room for new timestamp query. Max in flight unresolved queries reached");
    }

    QueryHandle handle = QueryHandle::CreateHandle(head, generation);
    inFlightQueries[head].handle = handle;
    inFlightQueries[head].queueType = queueType;
    inFlightQueries[head].token = GInfiniteSyncTokens[queueType];

    // advance head
    u32 prevHead = head;
    head = nextHead;

    // advance generation
    if (head < prevHead)
    {
        generation++;
        CleanupOldQueries();
    }

    return handle;
}

std::expected<Query, QueryStatus> RHITimestampQueryPoolBase::GetQueryData(QueryHandle handle)
{
    if (const auto it = resolvedQueries.find(handle); it != resolvedQueries.end())
    {
        return it->second;
    }

    if (handle.GetGeneration() < generation - 1)
    {
        return std::unexpected{ QueryStatus::OutOfDate };
    }

    return std::unexpected{ QueryStatus::NotReady };
}

void RHITimestampQueryPoolBase::CleanupOldQueries()
{
    std::vector<QueryHandle> handlesToCleanup;
    for (const auto& [handle, _] : resolvedQueries)
    {
        if (generation > 1)
        {
            if (handle.GetGeneration() < generation - 1)
            {
                handlesToCleanup.push_back(handle);
            }
        }
    }

    for (const auto& query : handlesToCleanup)
    {
        resolvedQueries.erase(query);
    }
}

RHITimestampQueryPoolBase::RHITimestampQueryPoolBase(RHI& rhi, RHIAllocator& allocator)
    : timestampBuffer{ rhi.CreateBuffer(allocator,
                                        { .name = "TimestampQueryReadback",
                                          .byteSize = sizeof(u64) * MaxInFlightTimestampCount,
                                          .usage = BufferUsage::GenericBuffer,
                                          .memoryLocality = ResourceMemoryLocality::CPURead }) }
    , rhi{ rhi }
{
}

void RHITimestampQueryPoolBase::FetchQueriesTimestamps(RHICommandList& cmdList, std::span<QueryHandle> handles)
{
    struct QueryRange
    {
        u32 begin;
        u32 count;
    };

    std::vector<QueryRange> ranges;

    u32 lastIndex = handles.begin()->GetIndex();
    QueryRange& currentRange = ranges.emplace_back(lastIndex, 1);
    for (auto it = std::next(handles.begin()); it != handles.end(); ++it)
    {
        if (it->GetIndex() != currentRange.begin + currentRange.count)
        {
            currentRange = ranges.emplace_back(it->GetIndex(), 1);
        }
        else
        {
            currentRange.count++;
        }
    }

    for (const auto& range : ranges)
    {
        cmdList.BufferBarrier(timestampBuffer, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest);
        cmdList.ResolveTimestampQueries(range.begin * 2, range.count * 2);
        cmdList.BufferBarrier(timestampBuffer, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource);
    }
}
void RHITimestampQueryPoolBase::UpdateSyncTokens(SyncToken token, std::span<QueryHandle> queries)
{
    for (QueryHandle query : queries)
    {
        inFlightQueries[query.GetIndex()].token = token;
    }
}

u32 RHITimestampQueryPoolBase::ResolveQueries()
{
    ResourceMappedMemory mem{ timestampBuffer };
    std::span mappedRange = { reinterpret_cast<u64*>(mem.GetMappedRange().data()), MaxInFlightTimestampCount };

    u32 resolvedQueriesCount = 0;

    bool stop = false;
    while (!stop && tail != head)
    {
        const InFlightQuery& inFlightQuery = inFlightQueries[tail];

        if (rhi->IsTokenComplete(inFlightQuery.token))
        {
            const u64* data = &mappedRange[inFlightQuery.handle.GetIndex() * 2];
            const u64 deltaTimestamp = data[1] - data[0];

            resolvedQueries.emplace(std::pair{
                inFlightQuery.handle,
                Query{ (deltaTimestamp / GetTimestampPeriod(inFlightQuery.queueType)) * 1000.0, deltaTimestamp } });

            tail = (tail + 1) % MaxInFlightQueriesCount;
            resolvedQueriesCount++;
        }
        else
        {
            stop = true;
        }
    }

    return resolvedQueriesCount;
}

RHIBuffer& RHITimestampQueryPoolBase::GetTimestampBuffer()
{
    return timestampBuffer;
}

} // namespace vex