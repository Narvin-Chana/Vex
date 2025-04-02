#pragma once

#include <string>

#include <comdef.h>

#include <Vex/Platform/Windows/WString.h>

namespace vex
{

inline std::string HRToError(HRESULT hr)
{
    _com_error err(hr);
    return err.ErrorMessage();
}

} // namespace vex
