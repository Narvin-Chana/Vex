#pragma once

#include <functional>

#ifndef __cpp_lib_move_only_function
// Fallback for environments with incomplete C++23 support
#include <Vex/Utility/Functional/move_only_function.h>
#endif

namespace vex
{

template <class T>
using MoveOnlyFunction =
#ifndef __cpp_lib_move_only_function
    std23::move_only_function<T>;
#else
    std::move_only_function<T>;
#endif

} // namespace vex