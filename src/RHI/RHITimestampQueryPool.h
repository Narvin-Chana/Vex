#pragma once
#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex
{
class GfxBackend;

struct Query
{
    // indexes in timestamp queries buffer
    uint32_t startIndex;
    uint32_t endIndex;
    bool ready;

    // Gets closed in EndQuery
    bool closed = false;
};

struct QueryHandle : Handle<QueryHandle>
{
};

struct QueryReadbackContext
{
};

class RHITimestampQueryPoolBase
{
    static constexpr u64 MaxQuerySlotsAtOnce = 10'000;

    FreeList<Query, QueryHandle> inFlightQueries;
    FreeListAllocator inflightQueriesIds;
    Buffer timestampBuffer;
    NonNullPtr<GfxBackend> graphics;

public:
    RHITimestampQueryPoolBase(GfxBackend& graphics);

    // Records beginning of query
    virtual QueryHandle BeginQuery() = 0;
    // Records end of querry
    virtual void EndQuery(QueryHandle handle) = 0;

    // Called when commandlist is closed
    // Makes timestamps available in the timestampBuffer at QueryHandle index for each query that were popped
    virtual void ResolveTimestamps() = 0;

    // Also invalidates QueryHandle
    u64 GetTimestampInterval(QueryHandle handle);
};

} // namespace vex
