#pragma once

#include <string>

#include <Vex/EnumFlags.h>
#include <Vex/Handle.h>
#include <Vex/Types.h>

namespace vex
{

enum class BufferUsage : u8
{
    StagingBuffer,
    VertexBuffer,
    IndexBuffer,
    GenericBuffer
};

// clang-format off

BEGIN_VEX_ENUM_FLAGS(BufferMemoryAccess, u8)
    CPURead,
    CPUWrite,
    GPURead,
    GPUWrite
END_VEX_ENUM_FLAGS();

// clang-format on

struct BufferDescription
{
    std::string name;
    u32 byteSize;
    BufferUsage usage;
    BufferMemoryAccess::Flags memoryAccess;
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