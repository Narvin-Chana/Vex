#pragma once

#include <format>
#include <vector>

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

#if !VEX_MODULES

// Magic_enum's formatter overload resolves compatible enums automatically.
#include <magic_enum/magic_enum_format.hpp>

#else

// Magic_enum currently does not work correctly with Vex's auto formatter due to modules.
// Until we figure out a solution, enums will return the underlying enum value converted to a string.

// Magic_enum also exposes a cpp module, which unfortunately collides with our #includes of the STL, rendering it also
// incompatible.
// i.e. 'import magic_enum;'

constexpr auto EnumToString(auto val)
{
    return std::to_string(std::to_underlying(val));
}

template <class E>
    requires(std::is_enum_v<E> and not std::is_same_v<E, std::byte>)
struct std::formatter<E> : std::formatter<std::string_view>
{
    constexpr auto format(E obj, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(EnumToString(obj), ctx);
    }
};

#endif
