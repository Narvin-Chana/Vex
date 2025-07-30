#pragma once

#include <string>

#include <winstring.h>

#include <Vex/Types.h>

namespace vex
{

inline std::string WStringToString(const std::wstring& wstr)
{
    std::string s;
    s.resize(wstr.size());
    i32 usedDefaultChar = 0;
    WideCharToMultiByte(CP_ACP,
                        0,
                        wstr.c_str(),
                        static_cast<i32>(wstr.size()),
                        s.data(),
                        static_cast<i32>(s.size()),
                        "#",
                        &usedDefaultChar);
    return s;
}

inline std::wstring StringToWString(const std::string& str)
{
    std::wstring s;
    s.resize(str.size());
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<i32>(str.size()), s.data(), static_cast<i32>(s.size()));
    return s;
}

} // namespace vex