#pragma once

#include <magic_enum/magic_enum.hpp>

// Base macro for combining hashes
#define VEX_HASH_COMBINE(seed, value)                                                                                  \
    (seed) ^= std::hash<std::remove_const_t<std::remove_reference_t<decltype(value)>>>()(value) + 0x9e3779b9 +         \
              ((seed) << 6) + ((seed) >> 2)

// Macro for hashing an enum using the string provided by magic_enum
#define VEX_HASH_COMBINE_ENUM(seed, enum_value)                                                                        \
    (seed) ^= std::hash<std::string>()(std::string(magic_enum::enum_name(enum_value))) + 0x9e3779b9 + ((seed) << 6) +  \
              ((seed) >> 2)

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
