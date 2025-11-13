#pragma once

#include <Vex/Hash.h>
#include <Vex/QueueType.h>
#include <Vex/Types.h>

namespace vex
{

struct SyncToken
{
    QueueType queueType = QueueType::Graphics;
    u64 value = 0;

    constexpr bool operator==(const SyncToken& other) const = default;
};

static constexpr std::array GInfiniteSyncTokens{
    SyncToken{ .queueType = QueueType::Copy, .value = std::numeric_limits<u64>::max() },
    SyncToken{ .queueType = QueueType::Compute, .value = std::numeric_limits<u64>::max() },
    SyncToken{ .queueType = QueueType::Graphics, .value = std::numeric_limits<u64>::max() },
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::SyncToken, 
    VEX_HASH_COMBINE(seed, obj.queueType);
    VEX_HASH_COMBINE(seed, obj.value);
);

// clang-format on
