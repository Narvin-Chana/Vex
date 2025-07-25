#pragma once

// Defines platform agnostic debug macros such as assert or debug break.

#if !VEX_SHIPPING

#if defined(_WIN32)
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

#define VEX_ASSERT(cond, ...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            VEX_DEBUG_BREAK();                                                                                         \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

#define VEX_NOT_YET_IMPLEMENTED() VEX_ASSERT(false, "Not yet implemented...")

#else

#define VEX_DEBUG_BREAK()
#define VEX_ASSERT(cond, ...)
#define VEX_NOT_YET_IMPLEMENTED()

#endif
