#pragma once

#include <type_traits>

#include <Vex/Types.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

template <class ValueT,
          std::size_t IndexBitSize,
          std::size_t GenerationBitSize,
          bool HasGeneration = (GenerationBitSize > 0)>
struct DebugHandleInfo
{
    ValueT index : IndexBitSize;
};

template <class ValueT, std::size_t IndexBitSize, std::size_t GenerationBitSize>
struct DebugHandleInfo<ValueT, IndexBitSize, GenerationBitSize, true>
{
    ValueT index : IndexBitSize;
    ValueT generation : GenerationBitSize;
};

template <class Derived, class ValueT, std::size_t IndexBitSize>
    requires((sizeof(ValueT) * 8 >= IndexBitSize) and std::unsigned_integral<ValueT>)
struct Handle
{
    using ValueType = ValueT;
    static constexpr std::size_t GenerationBitSize = sizeof(ValueT) * 8 - IndexBitSize;
    static constexpr bool HasGeneration = GenerationBitSize > 0;

    union
    {
        ValueType value = MaxValue;
#if !VEX_SHIPPING
        // Should just be used for debugging purposes, bitfield packing order is implementation-defined. Meaning no
        // actual logic should ever directly access these fields.
        DebugHandleInfo<ValueT, IndexBitSize, GenerationBitSize> debugHandleInfo;
#endif
    };

    constexpr static Derived CreateHandle(ValueType index, ValueType generation)
        requires(HasGeneration)
    {
        Derived handle;
        handle.SetHandle(index, generation);
        return handle;
    }
    constexpr void SetHandle(ValueType index, ValueType generation)
        requires(HasGeneration)
    {
        value = 0;
        value |= index & (MaxValue >> GenerationBitSize);
        value |= generation << IndexBitSize;
    }
    constexpr ValueType GetIndex() const
    {
        if constexpr (HasGeneration)
        {
            return value & (MaxValue >> GenerationBitSize);
        }
        return value;
    }
    constexpr ValueType GetGeneration() const
        requires(HasGeneration)
    {
        return value >> IndexBitSize;
    }
    explicit constexpr operator ValueType() const
    {
        return value;
    }
    constexpr bool operator==(Handle other) const
    {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool IsValid() const
    {
        return value != MaxValue;
    }

private:
    static constexpr ValueType MaxValue = std::numeric_limits<ValueType>::max();
};

// 32 bit handle
template <class Derived>
using Handle32 = Handle<Derived, u32, 24>;

// 64 bit handle
template <class Derived>
using Handle64 = Handle<Derived, u64, 32>;

template <class T>
concept HandleType = std::derived_from<T, Handle32<T>> or std::derived_from<T, Handle64<T>>;

} // namespace vex

template <vex::HandleType T>
struct std::hash<T>
{
    size_t operator()(const T& obj) const
    {
        size_t seed = 0;
        VEX_HASH_COMBINE(seed, obj.value);
        return seed;
    }
};

template <vex::HandleType T>
struct std::formatter<T>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const T& obj, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", obj.value);
    }
};