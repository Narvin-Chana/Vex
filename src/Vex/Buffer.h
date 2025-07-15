#pragma once

#include "EnumFlags.h"

#include <limits>
#include <string>

#include <Vex/Handle.h>
#include <Vex/Types.h>

namespace vex
{

enum class BufferUsage : u8
{
    VertexBuffer,
    IndexBuffer,
    GenericBuffer
};

BEGIN_VEX_ENUM_FLAGS(BufferMemoryAccess, u8)
CPURead, CPUWrite, GPURead,
    GPUWrite END_VEX_ENUM_FLAGS()

        struct BufferDescription
{
    std::string name;
    u32 size;
    BufferUsage usage;
    BufferMemoryAccess::Flags memoryAcces;
};

struct BufferView
{
    // TODO: implement buffers
};

// Strongly defined type represents a buffer.
// We use a struct (instead of a typedef/using) to enforce compile-time correctness of handles.
struct BufferHandle : public Handle<BufferHandle>
{
};

static constexpr BufferHandle GInvalidBufferHandle;

struct Buffer final
{
    BufferHandle handle;
    BufferDescription description;
};

} // namespace vex