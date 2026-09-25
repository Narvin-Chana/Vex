#pragma once

#include <array>
#include <string>
#include <utility>

#include <Vex/Types.h>
#include <Vex/Utility/Formattable.h>
#include <VexMacros.h>

namespace vex
{

template <typename T>
constexpr auto PurifyHashValue(const T& obj)
{
    if constexpr (std::is_enum_v<T>)
    {
        // For enums, convert to underlying type
        return std::to_underlying(obj);
    }
    else
    {
        // For primitive types and other hashable types
        return obj;
    }
}

struct SHA1HashDigest
{
    std::array<u32, 5> value{};
    constexpr bool operator==(const SHA1HashDigest&) const = default;
};
std::string HashToString(const SHA1HashDigest& hash);

} // namespace vex

// Base macro for combining hashes
#define VEX_HASH_COMBINE(seed, value)                                                                                  \
    (seed) ^= std::hash<std::decay_t<decltype(vex::PurifyHashValue(value))>>{}(vex::PurifyHashValue(value)) +          \
              0x9e3779b9 + ((seed) << 6) + ((seed) >> 2);

// Macro for hashing a container (vector, array, etc.)
#define VEX_HASH_COMBINE_CONTAINER(seed, container)                                                                    \
    for (const auto& item : container)                                                                                 \
    {                                                                                                                  \
        VEX_HASH_COMBINE(seed, item);                                                                                  \
    }

// Macro to generate hash function for a struct or class
#define VEX_MAKE_HASHABLE(type, fields)                                                                                \
    template <>                                                                                                        \
    struct std::hash<type>                                                                                             \
    {                                                                                                                  \
        size_t operator()(const type& obj) const                                                                       \
        {                                                                                                              \
            size_t seed = 0;                                                                                           \
            fields;                                                                                                    \
            return seed;                                                                                               \
        }                                                                                                              \
    };

VEX_MAKE_HASHABLE(vex::SHA1HashDigest, VEX_HASH_COMBINE_CONTAINER(seed, obj.value););
VEX_FORMATTABLE(vex::SHA1HashDigest, "{}", vex::HashToString(obj));
