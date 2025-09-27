#pragma once

#include <Vex/QueueType.h>
#include <Vex/Hash.h>
#include <Vex/Types.h>

namespace vex
{

struct SyncToken
{
    QueueType queueType = QueueType::Graphics;
    u64 value = 0;

    constexpr bool operator==(const SyncToken& other) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::SyncToken, 
    VEX_HASH_COMBINE(seed, obj.queueType);
    VEX_HASH_COMBINE(seed, obj.value);
);

// clang-format on
