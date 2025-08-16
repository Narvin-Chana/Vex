#pragma once

#include <GLFW/glfw3.h>
#include <math.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#if defined(__linux__)
// Undefine/define problematic X11 macros
#ifdef Always
#undef Always
#endif
#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#ifndef Bool
#define Bool bool
#endif
#endif

#include <GLFW/glfw3native.h>