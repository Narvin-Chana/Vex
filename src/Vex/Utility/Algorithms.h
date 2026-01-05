#pragma once

#include <utility>

#include <Vex/Types.h>

namespace vex
{

inline bool DoesRangeOverlap(u32 aStart, u32 aLength, u32 bStart, u32 bLength)
{
    if (aStart > bStart)
    {
        std::swap(aStart, bStart);
        std::swap(aLength, bLength);
    }
    return aStart + aLength >= bStart && aStart + aLength <= bStart + bLength;
};

} // namespace vex
