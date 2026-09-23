#pragma once

#include <Vex/Containers/beman/inplace_vector.hpp>

namespace vex
{

// Variable size array, allocated on the stack (backed by a std::array).
// Useful for when the upper limit of a vector is known at compile-time to avoid dynamic memory allocation.
template <class T, std::size_t N>
using InlineVector = beman::inplace_vector::inplace_vector<T, N>;

} // namespace vex