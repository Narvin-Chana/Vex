#pragma once

#ifdef _WIN32
#include <wtypes.h>
#endif

#include <Types.h>

namespace vex
{

#ifdef _WIN32
using PlatformWindowHandle = HWND;
#elif __linux__
// TODO: FIGURE OUT THE HANDLE TYPE ON LINUX
using PlatformWindowHandle = int;
#endif

struct PlatformWindow
{
    // Handle to the platform-specific window
    PlatformWindowHandle windowHandle;
    u32 width, height;
};

} // namespace vex
