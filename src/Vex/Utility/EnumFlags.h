#pragma once

#include <Vex/Utility/Hash.h>

namespace vex
{

// Formatted more compactly, so we diable clang-format.

// clang-format off

template <class E>
concept BitEnum = std::is_enum_v<E>;

// Defines an enum flag (bitset), underlying enum type should be a bit enum.
template <BitEnum E>
struct Flags
{
    using Underlying = std::underlying_type_t<E>;

    constexpr Flags() = default;
    constexpr Flags(E val) : data{static_cast<Flags>(val)} {}
    constexpr Flags(const Flags& other) : data{other.data} {}
    constexpr Flags& operator=(const Flags& other) { data = other.data; return *this;}
    constexpr Flags(Flags&& other) : data{std::move(other.data)} {}
    constexpr Flags& operator=(Flags&& other) { data = std::move(other.data); return *this;}
    constexpr ~Flags() = default;

    constexpr Flags& operator=(E other) { data = static_cast<Flags>(other); return *this; }

    [[nodiscard]] constexpr Flags operator|(Flags other) const { return Flags{data | other.data}; }
    [[nodiscard]] constexpr Flags operator&(Flags other) const { return Flags{data & other.data}; }
    [[nodiscard]] constexpr Flags operator^(Flags other) const { return Flags{data ^ other.data}; }
    [[nodiscard]] constexpr Flags operator~()            const { return Flags{~data}; }

    [[nodiscard]] constexpr Flags operator|(E other) const { return *this | Flags{other}; }
    [[nodiscard]] constexpr Flags operator&(E other) const { return *this & Flags{other}; }
    [[nodiscard]] constexpr Flags operator^(E other) const { return *this ^ Flags{other}; }

    constexpr Flags& operator|=(Flags other)    { data |= other.data; return *this; }
    constexpr Flags& operator&=(Flags other)    { data &= other.data; return *this; }
    constexpr Flags& operator^=(Flags other)    { data ^= other.data; return *this; }
    constexpr Flags& operator|=(E other)  { return *this |= Flags{other}; }
    constexpr Flags& operator&=(E other)  { return *this &= Flags{other}; }
    constexpr Flags& operator^=(E other)  { return *this ^= Flags{other}; }

    [[nodiscard]] constexpr bool IsSet(E bit) const { return (*this & bit) != 0; }
    [[nodiscard]] constexpr bool IsEmpty() const { return data == 0; }

    constexpr Flags& Clear() { data = {}; return *this; }

    constexpr Flags& Set(E bit)   { data |= bit.data; return *this; }
    constexpr Flags& Clear(E bit) { data &= ~bit.data; return *this; }
    constexpr Flags& Flip(E bit)  { data ^= bit.data; return *this; }

    [[nodiscard]] constexpr bool operator==(const Flags& other) const = default;
    constexpr operator bool() const { return !IsEmpty(); }

    Underlying data{};
};

template<BitEnum E>
[[nodiscard]] constexpr Flags<E> operator|(E lhs, E rhs) noexcept { return Flags<E>{lhs} | rhs; }
template<BitEnum E>
[[nodiscard]] constexpr Flags<E> operator&(E lhs, E rhs) noexcept { return Flags<E>{lhs} & rhs; }
template<BitEnum E>
[[nodiscard]] constexpr Flags<E> operator^(E lhs, E rhs) noexcept { return Flags<E>{lhs} ^ rhs; }
template<BitEnum E>
[[nodiscard]] constexpr Flags<E> operator~(E val)        noexcept { return ~Flags<E>{val}; }

} // namespace vex

template<vex::BitEnum E>
struct std::hash<vex::Flags<E>>
{
    constexpr size_t operator()(vex::Flags<E> f) const noexcept
    {
        return std::hash<typename vex::Flags<E>::underlying_t>{}(f.data);
    }
};

// clang-format on
