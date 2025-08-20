#pragma once

#include <type_traits>

#include <Vex/Hash.h>
#include <Vex/Types.h>

namespace vex
{
template <class Derived>
struct Handle
{
    // First 24 bits indicate handle index (max of 16'777'215), following 8 bits indicate the generation (max of
    // 255).
    u32 value = ~0U;

    static Derived CreateHandle(u32 index, u8 generation)
    {
        Derived handle;
        handle.SetHandle(index, generation);
        return handle;
    }
    void SetHandle(u32 index, u8 generation)
    {
        value = 0;
        value |= (index & 0x00FFFFFF);
        value |= static_cast<u32>(generation) << 24;
    }
    u32 GetIndex() const
    {
        return value & 0x00FFFFFF;
    }
    u8 GetGeneration() const
    {
        return value >> 24;
    }
    bool operator==(Handle other) const
    {
        return value == other.value;
    }
};

} // namespace vex

template <class T>
    requires std::derived_from<T, vex::Handle<T>>
struct std::hash<T>
{
    size_t operator()(const T& obj) const
    {
        size_t seed = 0;
        VEX_HASH_COMBINE(seed, obj.value);
        return seed;
    }
};
