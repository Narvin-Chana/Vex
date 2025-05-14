#pragma once

#include <limits>
#include <string>

#include <Vex/Handle.h>
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
struct BufferHandle : public Handle<BufferHandle>
{
};

static constexpr BufferHandle GInvalidBufferHandle;

struct Buffer final
{
    const BufferHandle handle;
    const BufferDescription description;
};

} // namespace vex