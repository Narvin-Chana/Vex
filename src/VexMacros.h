#pragma once

#if !VEX_MODULES
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_format.hpp>
#endif

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
        auto format(const type& obj, std::format_context& ctx) const                                                   \
        {                                                                                                              \
            return std::format_to(ctx.out(), formatStr, __VA_ARGS__);                                                  \
        }                                                                                                              \
    }

// Runtime validation, this is always performed (no matter the optimization level) compared to VEX_ASSERT which is
// stripped out when not in Debug.
#define VEX_CHECK(condition, fmt, ...)                                                                                 \
    if (!(condition))                                                                                                  \
    {                                                                                                                  \
        VEX_LOG(vex::LogLevel::Fatal, fmt, ##__VA_ARGS__);                                                             \
    }

// Doing logging with macros instead of with a function allows for DebugBreak to break in the actual code, avoiding us
// having to move up once in the call stack to get to the the actual code causing the error.

// Logs a potentially formatted string with one of the following log levels: Info, Warning, Error, Fatal.
// This follows std::format()'s formatting.
#define VEX_LOG(level, message, ...)                                                                                   \
    if ((level) >= vex::Logger::GetLogLevelFilter())                                                                   \
    {                                                                                                                  \
        vex::GLogger.Log((level), message, ##__VA_ARGS__);                                                             \
        if ((level) == vex::LogLevel::Fatal) /* Fatal error! Must exit. */                                             \
        {                                                                                                              \
            VEX_DEBUG_BREAK();                                                                                         \
            std::abort();                                                                                              \
        }                                                                                                              \
    }
