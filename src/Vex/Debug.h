#pragma once

// Defines platform agnostic debug macros such as assert or debug break.

#if !VEX_SHIPPING

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define VEX_DEBUG_BREAK() DebugBreak()
#elif defined(__linux__)
#include <signal.h>
#define VEX_DEBUG_BREAK() raise(SIGTRAP)
#elif defined(__clang__) || defined(__GNUC__)
#define VEX_DEBUG_BREAK() __builtin_trap()
#else // Should never happen...
#define VEX_DEBUG_BREAK() ((void)0)
#endif

#else

#define VEX_DEBUG_BREAK()

#endif