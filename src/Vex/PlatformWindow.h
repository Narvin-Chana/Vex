#pragma once

#if defined(_WIN32)
#include <wtypes.h>
#elif defined(__linux__)
#include <X11/X.h>
#include <X11/Xlib.h>
#undef None
#endif

#include <Vex/Types.h>

namespace vex
{

struct PlatformWindowHandle
{
#if defined(_WIN32)
    HWND window;
#elif defined(__linux__)
    Window window;
    Display* display;
#endif
};

struct PlatformWindow
{
    // Handle to the platform-specific window
    PlatformWindowHandle windowHandle;
    u32 width, height;
};

} // namespace vex
