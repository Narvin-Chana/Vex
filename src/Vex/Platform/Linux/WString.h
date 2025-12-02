#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return "";
    i32 size_needed = wcstombs(nullptr, wstr.c_str(), 0);
    if (size_needed <= 0)
        return "";
    std::vector<char> buffer(size_needed + 1);
    wcstombs(buffer.data(), wstr.c_str(), size_needed);
    return std::string(buffer.data());
}

std::wstring StringToWString(const std::string& str)
{
    if (str.empty())
        return L"";
    i32 size_needed = mbstowcs(nullptr, str.c_str(), 0);
    if (size_needed <= 0)
        return L"";
    std::vector<wchar_t> buffer(size_needed + 1);
    mbstowcs(buffer.data(), str.c_str(), size_needed);
    return std::wstring(buffer.data());
}

} // namespace vex