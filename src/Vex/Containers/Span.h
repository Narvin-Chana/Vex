#pragma once

#include <span>

namespace vex
{

template <class T, std::size_t Extent = std::dynamic_extent>
struct Span : public std::span<T, Extent>
{
    using Base = std::span<T, Extent>;
    using Base::span; // Inherit base constructors
    using V = std::remove_cv_t<T>;

    // Allow construction from std::span.
    constexpr Span(std::span<T, Extent> s) noexcept
        : Base(s)
    {
    }

    // Allows for a span to be constructed from an initializer_list, allowing for the following construction syntax for
    // a function taking in Span<const u32>: MyFoo({ 1, 2, 3}); This will be added to the standard library in C++26.
    constexpr explicit(Extent != std::dynamic_extent) Span(std::initializer_list<V> il) noexcept
        requires std::is_const_v<T>
        : Base(std::data(il), std::size(il))
    {
    }
};

} // namespace vex