#pragma once

#include <type_traits>

#include <Vex/Hash.h>
#include <Vex/Types.h>

namespace vex
{
template <class Derived = void>
struct Handle
{
    using HandleType = std::conditional_t<std::is_void_v<Derived>, Handle, Derived>;

    // First 24 bits indicate handle index (max of 16'777'215), following 8 bits indicate the generation (max of
    // 255).
    u32 value = ~0U;

    static HandleType CreateHandle(u32 index, u8 generation)
    {
        HandleType handle;
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

VEX_MAKE_HASHABLE(vex::Handle<>, VEX_HASH_COMBINE(seed, obj.value););