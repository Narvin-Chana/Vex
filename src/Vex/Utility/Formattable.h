#pragma once

#include <format>
#include <vector>

#include <Vex/Utility/WString.h>
#include <VexMacros.h>

// We do not override std::formatter with std::wstring as it would be IFNDR due to this rule from the STD:
// "[namespace.std]/2 permits specialising standard templates only if the declaration depends on a program-defined
// type."
// Instead we provide a wrapper with non-explicit constructor allowing for automatic formatting.

namespace vex
{

struct WStringView
{
    WStringView(const std::wstring& s)
        : str(s)
    {
    }
    const std::wstring& str;
};

} // namespace vex

#if defined(__cpp_lib_format_ranges) && __cpp_lib_format_ranges >= 202207L
// The library formats ranges (std::vector, std::array, ...) itself
#else
template <typename T, typename Alloc, typename CharT>
struct std::formatter<std::vector<T, Alloc>, CharT>
{
    std::formatter<T, CharT> elementFormatter;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        // The spec after ':' applies to each element
        return elementFormatter.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const std::vector<T, Alloc>& vec, FormatContext& ctx) const
    {
        auto out = ctx.out();
        *out++ = CharT('[');

        bool isFirst = true;
        for (const T& elem : vec)
        {
            if (!isFirst)
            {
                *out++ = CharT(',');
                *out++ = CharT(' ');
            }
            isFirst = false;

            ctx.advance_to(out);
            out = elementFormatter.format(elem, ctx);
        }

        *out++ = CharT(']');
        return out;
    }
};
#endif

VEX_FORMATTABLE(vex::WStringView, "{}", vex::PlatformUtil::WStringToString(obj.str));