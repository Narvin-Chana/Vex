#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

inline std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return "";
    i32 sizeNeeded = wcstombs(nullptr, wstr.c_str(), 0);
    if (sizeNeeded <= 0)
        return "";
    std::vector<char> buffer(sizeNeeded + 1);
    wcstombs(buffer.data(), wstr.c_str(), sizeNeeded);
    return std::string(buffer.data());
}

inline std::wstring StringToWString(const std::string& str)
{
    if (str.empty())
        return L"";
    i32 sizeNeeded = mbstowcs(nullptr, str.c_str(), 0);
    if (sizeNeeded <= 0)
        return L"";
    std::vector<wchar_t> buffer(sizeNeeded + 1);
    mbstowcs(buffer.data(), str.c_str(), sizeNeeded);
    return std::wstring(buffer.data());
}

} // namespace vex