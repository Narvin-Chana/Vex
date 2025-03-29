#pragma once

#include <cstdlib>
#include <print>

#include <Vex/Debug.h>
#include <Vex/Types.h>

namespace vex
{

// TODO: Improvements to logger will be made in another MR. This is just to get something simple working.
enum LogLevel : u8
{
    Info,
    Warning,
    Error,
    Fatal
};

// Calls to log with a lower level than this will be ignored.
inline LogLevel GLogLevelFilter = Info;

} // namespace vex

#define VEX_LOGGING_NONE 0
#define VEX_LOGGING_IOSTREAM (1 << 0)
#define VEX_LOGGING_FILE (1 << 1)

// TODO: Set this via CMake options? Or should be changeable via global func? Will be improved further in a logging
// specific MR.
#define VEX_LOGGING_TYPE (VEX_LOGGING_IOSTREAM | VEX_LOGGING_FILE)

#if VEX_LOGGING_TYPE != 0

// Doing logging like this instead of with a function allows for two things:
//      1. DebugBreak will break in the actual code, avoiding us having to move up once in the stack to get the the
//      actual code causing the error.
//      2. std::print contains compile-time verification of the print arguments, this will avoid problems with
//      formatting.
#define VexLog(level, message, ...)                                                                                    \
    if (level >= vex::GLogLevelFilter)                                                                                 \
    {                                                                                                                  \
        std::print(message, __VA_ARGS__);                                                                              \
        if (level == Fatal) /* Fatal error! Must exit. */                                                              \
        {                                                                                                              \
            VEX_DEBUG_BREAK();                                                                                         \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    }
#else
#define VexLog(level, message, ...)
#endif