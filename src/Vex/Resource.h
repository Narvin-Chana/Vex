#pragma once

#include <Vex/Handle.h>
#include <Vex/Types.h>

namespace vex
{

enum class ResourceLifetime : u8
{
    Static,  // Lives across many frames.
    Dynamic, // Valid only for the current frame.
};

enum class ResourceMemoryLocality : u8
{
    GPUOnly,
    CPURead,
    CPUWrite,
};

struct BindlessHandle : Handle<BindlessHandle>
{
};

static constexpr BindlessHandle GInvalidBindlessHandle;

} // namespace vex