#include "HRChecker.h"

#include <Vex/Logger.h>
#include <Vex/Platform/Windows/HResult.h>

namespace vex::dx12
{

HrGrabber::HrGrabber(HRESULT hr, std::source_location loc)
    : hr(hr)
    , loc(std::move(loc))
{
}

void operator>>(const HrGrabber& grabber, CheckerToken)
{
    if (FAILED(grabber.hr))
    {
        VEX_LOG(Fatal, "Graphics Error: {} {}({})", HRToError(grabber.hr), grabber.loc.file_name(), grabber.loc.line());
    }
}

void operator<<(CheckerToken checker, const HrGrabber& grabber)
{
    grabber >> checker;
}

std::expected<void, std::string> operator>>(const HrGrabber& grabber, CheckerTokenSoft)
{
    if (FAILED(grabber.hr))
    {
        return std::unexpected(std::format("Graphics Error: {} {}({})",
                                           HRToError(grabber.hr),
                                           grabber.loc.file_name(),
                                           grabber.loc.line()));
    }

    return {};
}

std::expected<void, std::string> operator<<(CheckerTokenSoft checker, const HrGrabber& grabber)
{
    return grabber >> checker;
}

} // namespace vex::dx12