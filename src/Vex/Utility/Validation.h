#pragma once

#include <Vex/Logger.h>

// Runtime validation, this is always performed (no matter the optimization level) compared to VEX_ASSERT which is
// stripped out when not in Debug.
#define VEX_CHECK(condition, fmt, ...)                                                                                 \
    if (!(condition))                                                                                                  \
    {                                                                                                                  \
        VEX_LOG(vex::LogLevel::Fatal, fmt, ##__VA_ARGS__);                                                             \
    }
