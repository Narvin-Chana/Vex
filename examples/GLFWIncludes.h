#pragma once

// Only the portable GLFW API. The native (glfw3native.h) part is confined to ExampleApplication.cpp, since its
// platform headers (X11 especially) define macros that collide with Vex identifiers.
#include <GLFW/glfw3.h>
