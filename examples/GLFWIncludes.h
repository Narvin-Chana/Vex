#pragma once

// Portable GLFW API plus the platform-specific (glfw3native.h) API. Only include this header if you need the native
// API, otherwise include <GLFW/glfw3.h> directly.
//
// The native headers must come before anything that imports Vex: GCC conflicts with the Xlib declarations already
// inside the module. The X11 macros that collide with Vex identifiers are removed afterwards.
#include <cmath>
#include <functional>

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3native.h>

#if defined(__linux__)
// Undefine problematic X11 macros
#undef Always
#undef Bool
#undef False
#undef None
#undef Status
#undef Success
#undef True
#endif
