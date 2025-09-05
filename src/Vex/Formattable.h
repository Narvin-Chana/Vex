#pragma once

#include <format>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Platform/Platform.h>

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

// This could be elsewhere
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

template <class E>
    requires std::is_enum_v<E>
struct std::formatter<E>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(E obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}", magic_enum::enum_name(obj));
    }
};
