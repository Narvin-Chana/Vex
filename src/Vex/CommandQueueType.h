#pragma once

#include <Vex/Types.h>

namespace vex
{

namespace CommandQueueTypes
{
enum Value : u8
{
    // Transfer-only operations
    Copy = 0,
    // Compute operations (includes Copy capabilites)
    Compute = 1,
    // Graphics operations (includes Compute and Copy capabilities)
    Graphics = 2,
};
// Not using a Count enum value allows us to more easily iterate over the CommandQueueTypes with magic_enum.
static constexpr u8 Count = 3;
} // namespace CommandQueueTypes

using CommandQueueType = CommandQueueTypes::Value;

} // namespace vex