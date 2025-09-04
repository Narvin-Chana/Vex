#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

// Determines how many frames should be in flight at once.
// More frames in flight means less GPU starvation, but also more input latency.
// Single buffering is not supported due to the APIs Vex supports not allowing for swapchains of less than 2
// backbuffers.
enum class FrameBuffering : u8
{
    // Two frames in flight at once
    Double = 2,
    // Three frames in flight at once
    Triple = 3
};

} // namespace vex