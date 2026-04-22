#pragma once

#include <format>
#include <vector>

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_format.hpp>

#include <Vex/Utility/WString.h>

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
