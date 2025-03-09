#pragma once

#include <Formats.h>
#include <PlatformWindow.h>

namespace vex
{

struct BackendDescription
{
    PlatformWindow platformWindow;
    Format swapChainFormat;
};

class GfxBackend* CreateGraphicsBackend(const BackendDescription& description);

class GfxBackend
{
};

} // namespace vex