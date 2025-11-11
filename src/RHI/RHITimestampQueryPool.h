#pragma once
#include <expected>

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/Synchronization.h>

namespace vex
{

enum class QueryStatus
{
    // Result was not returned by GPU yet
    NotReady,
    // Resolved query data was cleaned up due to long lifetime
    OutOfDate
};

struct Query
{
    double durationMs;
    u64 timestampInterval;
};

struct QueryHandle : Handle<QueryHandle>
{
};

static constexpr QueryHandle GInvalidQueryHandle;

class RHITimestampQueryPoolBase
{
protected:
    static constexpr u32 MaxInFlightQueriesCount = 10'000;
    // Each query has two timestamps: begin and end
    static constexpr u32 MaxInFlightTimestampCount = 2 * MaxInFlightQueriesCount;

    struct InFlightQuery
    {
        QueryHandle handle;
        SyncToken token;
        QueueType queueType;
    };

    std::array<InFlightQuery, MaxInFlightQueriesCount> inFlightQueries{};
    // Head = next free element in ring buffer
    // Tail = last filled element
    u32 head = 0, tail = 0;
    u32 generation = 0;

    std::unordered_map<QueryHandle, Query> resolvedQueries;
    RHIBuffer timestampBuffer;

    NonNullPtr<RHI> rhi;

    // Returns the tick rate in ticks/seconds
    virtual double GetTimestampPeriod(QueueType queueType) = 0;

    // Cleans up the resolved queries map of the generation - 2 queries. (They are very old)
    // This is mostly to save memory. We could store them forever if we decided to
    void CleanupOldQueries();

public:
    RHITimestampQueryPoolBase(RHI& rhi, RHIAllocator& allocator);

    // Transfer recorded data from GPU to mappable buffer for a specific set of queries
    void FetchQueriesTimestamps(RHICommandList& cmdList, std::span<QueryHandle> handles);
    // Updates the given queries with the provided sync token
    void UpdateSyncTokens(SyncToken token, std::span<QueryHandle> queries);

    RHIBuffer& GetTimestampBuffer();

    // Reads the queries that are completed from the mapped buffer memory
    u32 ResolveQueries();
    // Allocates a query in the ring buffer
    QueryHandle AllocateQuery(QueueType queueType);

    // Returns the values for that specific handle or a status on the state of it.
    std::expected<Query, QueryStatus> GetQueryData(QueryHandle handle);
};

} // namespace vex
