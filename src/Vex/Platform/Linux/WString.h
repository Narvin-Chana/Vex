#pragma once

#include <codecvt>
#include <locale>
#include <string>

namespace vex
{

inline std::string WStringToString(const std::wstring& wstr)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr);
}

inline std::wstring StringToWString(const std::string& str)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
}

} // namespace vex