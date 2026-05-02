#pragma once

#if !VEX_MODULES

// Magic_enum's formatter overload resolves compatible enums automatically.
#include <magic_enum/magic_enum_format.hpp>

#else

// Magic_enum exposes a cpp module which automatically implements std::formatter.

// MSVC dictates that in order to avoid multiple inclusions with their module implementation, you should include the STL
// FIRST then import your module.
#include <functional>
#include <optional>

import magic_enum;

#endif
