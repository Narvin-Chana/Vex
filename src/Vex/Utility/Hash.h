#pragma once

#include <iomanip>
#include <sstream>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Types.h>

namespace vex
{

template <typename T>
constexpr auto PurifyHashValue(const T& obj) -> auto
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

using SHA1HashDigest = std::array<u32, 5>;

inline std::string HashToString(const SHA1HashDigest& hash)
{
    std::ostringstream result;
    for (size_t i = 0; i < sizeof(hash) / sizeof(hash[0]); i++)
    {
        result << std::hex << std::setfill('0') << std::setw(8);
        result << hash[i];
    }
    return result.str();
}

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
