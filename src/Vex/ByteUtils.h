#pragma once

#include <cmath>
#include <tuple>
#include <type_traits>
#include <utility>

#include <Vex/Types.h>

namespace vex
{

// Alignment must be power of two.
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

template <class T>
    requires std::is_integral_v<T>
constexpr T DivRoundUp(T numerator, T denominator)
{
    return (numerator + denominator - 1) / denominator;
}

inline u8 ComputeMipCount(std::tuple<u32, u32, u32> dimensions)
{
    auto [width, height, depth] = dimensions;
    return static_cast<u8>(1 + std::floor(std::log2(std::max(std::max(width, height), depth))));
}

} // namespace vex