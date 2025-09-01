#pragma once
#include <string_view>

#include <Vex/Logger.h>

namespace vex
{

template <class... Ts>
void constexpr LogFailValidation(std::string_view fmt, Ts&&... args)
{
    VEX_LOG(Fatal, "Validation error: {}", std::vformat(fmt, std::make_format_args(args...)));
};

} // namespace vex