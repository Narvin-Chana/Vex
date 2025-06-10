#pragma once

#if defined(_WIN32)
#include <Vex/Platform/Windows/WString.h>
#elif defined(__linux__)
#include <Vex/Platform/Linux/WString.h>
#endif
