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

VEX_FORMATTABLE(vex::WStringView, "{}", vex::PlatformUtil::WStringToString(obj.str));