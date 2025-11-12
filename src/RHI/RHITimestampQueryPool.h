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
    // Increase this value if we reach the max Handle too fast.
    // Theorethical limit on Handle is 16,777,215 values
    static constexpr u32 MaxInFlightQueriesCount = 100;
    // Each query has two timestamps: begin and end
    static constexpr u32 MaxInFlightTimestampCount = 2 * MaxInFlightQueriesCount;

    struct InFlightQuery
    {
        SyncToken token;
        bool isRegistered = false;
    };
    FreeList<InFlightQuery, QueryHandle> inFlightQueries;

    struct RegisteredQuery
    {
        Query query;
        u32 generation;
    };
    std::unordered_map<QueryHandle, RegisteredQuery> resolvedQueries;

    RHIBuffer timestampBuffer;
    u32 generation;

    NonNullPtr<RHI> rhi;

    // Returns the tick rate in ticks/seconds
    virtual double GetTimestampPeriod(QueueType queueType) = 0;

    // Cleans up the resolved queries map of the generation - 2 queries. (They are very old)
    // This is mostly to save memory. We could store them forever if we decided to until we have handle limitations
    // This also flushes registered in flight queries to make them reusable later
    void CleanupQueries();
    // Copies the queries that are completed from the mapped buffer memory to the cache
    void ResolveQueries();

public:
    RHITimestampQueryPoolBase(RHI& rhi, RHIAllocator& allocator);

    // Transfer recorded data from GPU to mappable buffer for a specific set of queries on a command list
    void FetchQueriesTimestamps(RHICommandList& cmdList, std::span<QueryHandle> handles);
    // Updates the given queries with the provided sync token
    void UpdateSyncTokens(SyncToken token, std::span<QueryHandle> queries);
    // Allocates a query
    QueryHandle AllocateQuery(QueueType queueType);

    // Returns the values for that specific handle or a status on the state of it.
    std::expected<Query, QueryStatus> GetQueryData(QueryHandle handle);

    RHIBuffer& GetTimestampBuffer();
};

} // namespace vex
