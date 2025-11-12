#pragma once

#define VEX_CHECK(condition, fmt, ...)                                                                                 \
    if (!(condition))                                                                                                  \
    {                                                                                                                  \
        VEX_LOG(vex::LogLevel::Fatal, fmt, ##__VA_ARGS__);                                                             \
    }
