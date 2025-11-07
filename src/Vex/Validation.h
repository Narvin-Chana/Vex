#pragma once

#define VEX_CHECK(condition, fmt, ...)                                                                                 \
    if (!(condition))                                                                                                  \
    {                                                                                                                  \
        VEX_LOG(Fatal, fmt, ##__VA_ARGS__);                                                                            \
    }
