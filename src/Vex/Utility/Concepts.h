#pragma once

#include <concepts>
#include <Vex/Containers/Span.h>

#include <Vex/Types.h>

namespace vex
{

template <typename T>
concept IsContainer = requires(T a) {
    { a.begin() } -> std::input_or_output_iterator;
    { a.end() } -> std::input_or_output_iterator;
    typename T::value_type;
};

} // namespace vex