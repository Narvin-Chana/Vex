#pragma once

#include <type_traits>

namespace vex
{

template <class T>
    requires std::is_integral_v<T>
T AlignUp(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace vex