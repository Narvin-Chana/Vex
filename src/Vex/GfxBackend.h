#pragma once

#include <Vex/Formats.h>
#include <Vex/PlatformWindow.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct BackendDescription
{
    PlatformWindow platformWindow;
    Format swapChainFormat;
};

class GfxBackend
{
public:
    GfxBackend(const BackendDescription& description);

private:
    BackendDescription description;
};

UniqueHandle<GfxBackend> CreateGraphicsBackend(const BackendDescription& description);

} // namespace vex