#pragma once

#include <Vex/QueueType.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

struct SyncToken
{
    QueueType queueType = QueueType::Graphics;
    u64 value = 0;

    constexpr bool IsValid() const
    {
        return queueType != QueueTypes::Invalid && value != std::numeric_limits<u64>::max();
    }

    constexpr bool operator==(const SyncToken& other) const = default;
};

inline constexpr SyncToken GInvalidSyncToken{ .queueType = QueueType::Invalid };

inline constexpr std::array GInfiniteSyncTokens{
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
