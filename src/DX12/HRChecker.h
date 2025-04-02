#pragma once

#include <algorithm>
#include <expected>
#include <source_location>
#include <string>

#include <wrl.h>

namespace vex::dx12
{

// Thanks to ChiliTomatoNoodle for this great code!
// https://github.com/planetchili/d3d12-shallow
struct HrGrabber
{
    HrGrabber(HRESULT hr, std::source_location = std::source_location::current()) noexcept;
    HRESULT hr;
    std::source_location loc;
};

struct CheckerToken
{
};
void operator>>(const HrGrabber&, CheckerToken);
void operator<<(CheckerToken, const HrGrabber&);

struct CheckerTokenSoft
{
};
std::expected<void, std::string> operator>>(const HrGrabber&, CheckerTokenSoft);
std::expected<void, std::string> operator<<(CheckerTokenSoft, const HrGrabber&);

// Use this to check for HRESULT errors in DX12 functions, crashes if an error occurs.
inline CheckerToken chk;
// Use this to check for HRESULT errors in DX12 functions, without crashing.
inline CheckerTokenSoft chkSoft;

} // namespace vex::dx12