#pragma once

#include <Vex/Formats.h>
#include <Vex/PlatformWindow.h>

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

// Option A: Create method with hidden global variable
GfxBackend* CreateGraphicsBackend(const BackendDescription& description);
void DestroyGraphicsBackend(GfxBackend* backend);

// Option B: Create method w/ unique_ptr
// std::unique_ptr<GfxBackend> CreateGraphicsBackend(const BackendDescription& description);
// Shutdown is done in destructor

// Option C: Public constructor
// GfxBackend myBackend;

} // namespace vex