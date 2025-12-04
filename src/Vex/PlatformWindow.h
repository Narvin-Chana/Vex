#pragma once

#if defined(_WIN32)
#include <wtypes.h>
#elif defined(__linux__)
#include <X11/X.h>
#include <X11/Xlib.h>
#include <wayland-client.h>
#undef None
#endif

#include <variant>
#include <Vex/Types.h>

namespace vex
{

struct PlatformWindowHandle
{
#if defined(_WIN32)
    struct WindowsHandle
    {
        HWND window;
    };
#elif defined(__linux__)
    struct X11Handle
    {
        Window window;
        Display* display;
    };

    struct WaylandHandle
    {
        wl_surface* window;
        wl_display* display;
    };
#endif

    std::variant<
#if defined(_WIN32)
        WindowsHandle,
#elif defined(__linux__)
        X11Handle,
        WaylandHandle,
#endif
        std::monostate> handle;
};

struct PlatformWindow
{
    // Handle to the platform-specific window
    PlatformWindowHandle windowHandle;
    u32 width, height;
};

} // namespace vex
