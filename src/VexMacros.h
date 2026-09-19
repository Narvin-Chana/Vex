#pragma once

#include <Vex/Platform/Debug.h>

#if !VEX_SHIPPING

#define VEX_COMBINE1(X, Y) X##Y // helper macro
#define VEX_COMBINE(X, Y) VEX_COMBINE1(X, Y)

#define VEX_GPU_SCOPED_EVENT(ctx, name)                                                                                \
    auto VEX_COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedGPUEvent(name);
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)                                                                   \
    auto VEX_COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedGPUEvent(name, { r, g, b });

#else

#define VEX_GPU_SCOPED_EVENT(ctx, name)
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)

#endif

// Defines the std::formatter boilerplate for making a type printable using VEX_LOG (and std:format).
// formatStr should be the output format and the variadic should be the type's fields to output.
#define VEX_FORMATTABLE(type, formatStr, ...)                                                                          \
    template <>                                                                                                        \
    struct std::formatter<type>                                                                                        \
    {                                                                                                                  \
        constexpr auto parse(std::format_parse_context& ctx)                                                           \
        {                                                                                                              \
            return ctx.begin();                                                                                        \
        }                                                                                                              \
                                                                                                                       \
        auto format(const type& obj, auto& ctx) const                                                                  \
        {                                                                                                              \
            return std::format_to(ctx.out(), formatStr, __VA_ARGS__);                                                  \
        }                                                                                                              \
    }

// Runtime validation, this is always performed (no matter the optimization level) compared to VEX_ASSERT which is
// stripped out when not in Debug.
#define VEX_CHECK(condition, fmt, ...)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            VEX_LOG(vex::Fatal, fmt, ##__VA_ARGS__);                                                                   \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

// Doing logging with macros instead of with a function allows for DebugBreak to break in the actual code, avoiding us
// having to move up once in the call stack to get to the actual code causing the error.

// Logs a potentially formatted string with one of the following log levels: Info, Warning, Error, Fatal.
// This follows std::format()'s formatting.
#define VEX_LOG(level, message, ...)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((level) >= vex::Logger::GetLogLevelFilter())                                                               \
        {                                                                                                              \
            vex::GLogger.Log((level), message, ##__VA_ARGS__);                                                         \
            if ((level) == vex::Fatal) /* Fatal error! Must exit. */                                                   \
            {                                                                                                          \
                VEX_DEBUG_BREAK();                                                                                     \
                std::abort();                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

#if !VEX_SHIPPING

// Asserts are non-shipping checks.
#define VEX_ASSERT(cond, ...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            VEX_DEBUG_BREAK();                                                                                         \
            VEX_LOG(vex::Error, "Assertion `{}` failed at {}:{}", #cond, __FILE__, __LINE__);                          \
            VEX_LOG(vex::Error, "Assert message: " __VA_ARGS__);                                                       \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

#define VEX_NOT_YET_IMPLEMENTED() VEX_ASSERT(false, "Not yet implemented...")

#else

#define VEX_ASSERT(cond, ...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)sizeof(cond);                                                                                            \
    }                                                                                                                  \
    while (0)
#define VEX_NOT_YET_IMPLEMENTED()                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
    }                                                                                                                  \
    while (0)

#endif