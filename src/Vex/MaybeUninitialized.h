#pragma once

#include <optional>

namespace vex
{

template <class T>
using MaybeUninitialized = std::optional<T>;

} // namespace vex