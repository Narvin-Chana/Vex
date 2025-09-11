#pragma once

#include <cmath>
#include <tuple>
#include <type_traits>
#include <utility>

#include <Vex/Types.h>

namespace vex
{

template <class T>
    requires std::is_integral_v<T>
constexpr T AlignUp(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

template <class T>
    requires std::is_integral_v<T>
constexpr bool IsAligned(T value, T alignment)
{
    return value % alignment == 0;
}

inline u8 ComputeMipCount(std::tuple<u32, u32, u32> maxDimension)
{
    auto [width, height, depth] = maxDimension;
    return static_cast<u8>(1 + std::log2(std::max(std::max(width, height), depth)));
}

} // namespace vex