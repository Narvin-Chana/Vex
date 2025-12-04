#pragma once

namespace vex
{

// Defines an enum flag (bitset) inside a custom namespace to avoid it leaking.
#define BEGIN_VEX_ENUM_FLAGS(name, underlyingType)                                                                     \
    namespace name                                                                                                     \
    {                                                                                                                  \
    using Flags = underlyingType;                                                                                      \
    enum Type : Flags                                                                                                  \
    {

#define END_VEX_ENUM_FLAGS()                                                                                           \
    }                                                                                                                  \
    ;                                                                                                                  \
    }

} // namespace vex