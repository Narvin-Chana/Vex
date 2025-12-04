#pragma once

#include <Vex/Platform/Debug.h>
#include <Vex/Platform/PlatformWindow.h>

#if defined(_WIN32)
#include <Vex/Platform/Windows/HResult.h>
#elif defined(__linux__)
// Nothing for now...
#endif
