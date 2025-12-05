#pragma once

#include <type_traits>

#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

template <class Derived, class ValueT, std::size_t IndexBitSize, std::size_t GenerationBitSize = sizeof(ValueT) * 8 - IndexBitSize>
    requires(((sizeof(ValueT) * 8) - IndexBitSize == GenerationBitSize) and std::unsigned_integral<ValueT>)
struct Handle
{
    using ValueType = ValueT;

    ValueType value = MaxValue;

    constexpr static Derived CreateHandle(ValueType index, ValueType generation)
    {
        Derived handle;
        handle.SetHandle(index, generation);
        return handle;
    }
    constexpr void SetHandle(ValueType index, ValueType generation)
    {
        value = 0;
        value |= index & (MaxValue >> GenerationBitSize);
        value |= generation << IndexBitSize;
    }
    constexpr ValueType GetIndex() const
    {
        return value & (MaxValue >> GenerationBitSize);
    }
    constexpr ValueType GetGeneration() const
        requires(GenerationBitSize > 0)
    {
        return value >> IndexBitSize;
    }
    constexpr bool operator==(Handle other) const
    {
        return value == other.value;
    }
    constexpr bool IsValid() const
    {
        return value != MaxValue;
    }

private:
    static constexpr ValueType MaxValue = std::numeric_limits<ValueType>::max();
};

// 32 bit handle
template <class Derived>
using Handle32 = Handle<Derived, u32, 24, 8>;

// 64 bit handle
template <class Derived>
using Handle64 = Handle<Derived, u64, 32, 32>;

} // namespace vex

template <class T>
    requires std::derived_from<T, vex::Handle32<T>>
struct std::hash<T>
{
    size_t operator()(const T& obj) const
    {
        size_t seed = 0;
        VEX_HASH_COMBINE(seed, obj.value);
        return seed;
    }
};

template <class T>
    requires std::derived_from<T, vex::Handle64<T>>
struct std::hash<T>
{
    size_t operator()(const T& obj) const
    {
        size_t seed = 0;
        VEX_HASH_COMBINE(seed, obj.value);
        return seed;
    }
};
