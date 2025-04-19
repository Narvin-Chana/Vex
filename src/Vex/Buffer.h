#pragma once

#include <limits>
#include <string>

#include <Vex/Types.h>

namespace vex
{

struct BufferDescription
{
    std::string name;
    u32 size;
};

// Strongly defined type represents a buffer.
// We use a struct (instead of a typedef/using) to enforce compile-time correctness of handles.
struct BufferHandle
{
    u32 value = std::numeric_limits<u32>::max();
};

static constexpr BufferHandle GInvalidBufferHandle;

} // namespace vex