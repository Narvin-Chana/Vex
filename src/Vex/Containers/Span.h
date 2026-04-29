#pragma once

#include <span>
#include <type_traits>

namespace vex
{

template <class T, std::size_t Extent = std::dynamic_extent>
struct Span : public std::span<T, Extent>
{
    using Base = std::span<T, Extent>;
    using Base::Base; // Inherit base constructors
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

// Mirror all std::span deduction guides
template <class It, class End>
Span(It, End) -> Span<std::remove_reference_t<std::iter_reference_t<It>>>;

template <class T, std::size_t N>
Span(T (&)[N]) -> Span<T, N>;

template <class T, std::size_t N>
Span(std::array<T, N>&) -> Span<T, N>;

template <class T, std::size_t N>
Span(const std::array<T, N>&) -> Span<const T, N>;

template <class R>
Span(R&&) -> Span<std::remove_reference_t<std::ranges::range_reference_t<R>>>;

} // namespace vex