#pragma once

#include <cmath>
#include <type_traits>

namespace vex
{

template <class T>
    requires std::is_integral_v<T>
T AlignUp(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

inline u8 ComputeMipCount(std::tuple<u32, u32, u32> maxDimension)
{
    auto [width, height, depth] = maxDimension;
    return static_cast<u8>(1 + std::log2(std::max(std::max(width, height), depth)));
}

} // namespace vex