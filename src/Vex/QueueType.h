#pragma once

#include <Vex/Types.h>

namespace vex
{

namespace QueueTypes
{
enum Value : u8
{
    Invalid = 0xff,
    // Transfer-only operations
    Copy = 0,
    // Compute operations (includes Copy capabilites)
    Compute = 1,
    // Graphics operations (includes Compute and Copy capabilities)
    Graphics = 2,
};
// Not using a Count enum value allows us to more easily iterate over the QueueTypes with magic_enum.
static constexpr u8 Count = 3;
} // namespace QueueTypes

using QueueType = QueueTypes::Value;

} // namespace vex