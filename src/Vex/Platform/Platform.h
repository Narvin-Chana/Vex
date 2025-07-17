#pragma once

#if defined(_WIN32)

#include <Vex/Platform/Windows/WString.h>

#elif defined(__linux__)

#include <Vex/Platform/Linux/WString.h>

// X11 defines a macro called "Always"
#if defined(Always)
#undef Always
#endif // defined(Always)

#endif
