#include "RHITimestampQueryPool.h"

#include <Vex/Graphics.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/Utility/Validation.h>

namespace vex
{

QueryHandle RHITimestampQueryPoolBase::AllocateQuery(QueueType queueType)
{
    if (inFlightQueries.ElementCount() == MaxInFlightQueriesCount)
    {
        ResolveQueries();
        CleanupQueries();
        VEX_CHECK(inFlightQueries.ElementCount() != MaxInFlightQueriesCount,
                  "Unable to make room for new timestamp query. Max in flight unresolved queries reached");
    }

    return inFlightQueries.AllocateElement({ GInfiniteSyncTokens[queueType], false });
}

std::expected<Query, QueryStatus> RHITimestampQueryPoolBase::GetQueryData(QueryHandle handle)
{
    ResolveQueries();

    if (const auto it = resolvedQueries.find(handle); it != resolvedQueries.end())
    {
        return it->second.query;
    }

    if (inFlightQueries.IsValid(handle))
    {
        return std::unexpected{ QueryStatus::NotReady };
    }

    return std::unexpected{ QueryStatus::OutOfDate };
}

void RHITimestampQueryPoolBase::CleanupQueries()
{
    generation++;

    std::vector<QueryHandle> inFlightHandlesToFree;
    for (auto it = inFlightQueries.begin(); it != inFlightQueries.end(); ++it)
    {
        if (it->isRegistered)
        {
            inFlightHandlesToFree.push_back(it.GetHandle());
        }
    }

    for (const QueryHandle query : inFlightHandlesToFree)
    {
        inFlightQueries.FreeElement(query);
    }

    std::vector<QueryHandle> handlesToCleanup;
    for (const auto& [handle, query] : resolvedQueries)
    {
        if (generation > 1 && query.generation < generation - 1)
        {
            handlesToCleanup.push_back(handle);
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
    inFlightQueries.Resize(MaxInFlightQueriesCount);
}

void RHITimestampQueryPoolBase::FetchQueriesTimestamps(RHICommandList& cmdList, Span<QueryHandle> handles)
{
    struct QueryRange
    {
        u64 begin;
        u64 count;
    };

    // Will make sure we are as compact as possible
    std::sort(handles.begin(),
              handles.end(),
              [](const QueryHandle& a, const QueryHandle& b) { return a.GetIndex() < b.GetIndex(); });

    std::vector<QueryRange> ranges;
    u64 lastIndex = handles.begin()->GetIndex();
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
void RHITimestampQueryPoolBase::UpdateSyncTokens(SyncToken token, Span<const QueryHandle> queries)
{
    for (QueryHandle query : queries)
    {
        inFlightQueries[query].token = token;
    }
}

void RHITimestampQueryPoolBase::ResolveQueries()
{
    const MappedMemory mem{ timestampBuffer };
    std::span mappedRange = { reinterpret_cast<const u64*>(mem.GetMappedRange().data()), MaxInFlightTimestampCount };

    for (auto it = inFlightQueries.begin(); it != inFlightQueries.end(); ++it)
    {
        QueryHandle handle = it.GetHandle();
        if (!it->isRegistered && rhi->IsTokenComplete(it->token))
        {
            it->isRegistered = true;

            const u64* data = &mappedRange[handle.GetIndex() * 2];
            const u64 deltaTimestamp = data[1] - data[0];

            VEX_ASSERT(!resolvedQueries.contains(handle));
            resolvedQueries.emplace(
                std::pair{ handle,
                           RegisteredQuery{ Query{ (deltaTimestamp / GetTimestampPeriod(it->token.queueType)) * 1000.0,
                                                   deltaTimestamp },
                                            generation } });
        }
    }
}

RHIBuffer& RHITimestampQueryPoolBase::GetTimestampBuffer()
{
    return timestampBuffer;
}

} // namespace vex