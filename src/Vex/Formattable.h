#pragma once

#include "Platform/Platform.h"

#include <format>
#include <vector>

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

// Generic formatter for std::vector<T> where T is formattable
template <typename T>
struct std::formatter<std::vector<T>>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const std::vector<T>& vec, std::format_context& ctx) const
    {
        auto out = ctx.out();
        *out++ = '[';

        for (size_t i = 0; i < vec.size(); ++i)
        {
            if (i > 0)
            {
                *out++ = ',';
                *out++ = ' ';
            }
            out = std::format_to(out, "{}", vec[i]);
        }

        *out++ = ']';
        return out;
    }
};

// This could be elsewehere
template <>
struct std::formatter<std::wstring>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const std::wstring& obj, std::format_context& ctx) const
    {
        auto converted = vex::WStringToString(obj);
        return std::format_to(ctx.out(), "{}", converted);
    }
};
