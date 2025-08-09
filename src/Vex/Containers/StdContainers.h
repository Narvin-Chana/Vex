#pragma once
#include <vector>

#include <Vex/Types.h>

namespace vex
{

template <class T>
struct U32Alloc : std::allocator<T>
{
    using size_type = u32;
};

// This allows us to use u32 as size values and indices
template <class T>
using vector = std::vector<T, U32Alloc<T>>;

} // namespace vex