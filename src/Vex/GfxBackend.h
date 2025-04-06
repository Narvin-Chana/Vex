#pragma once

#include <Vex/Formats.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/PlatformWindow.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct RenderHardwareInterface;

struct BackendDescription
{
    PlatformWindow platformWindow;
    TextureFormat swapChainFormat;
    bool enableGPUDebugLayer = true;
    bool enableGPUBasedValidation = true;
};

class GfxBackend
{
public:
    GfxBackend(UniqueHandle<RenderHardwareInterface>&& newRHI, const BackendDescription& description);
    ~GfxBackend();

private:
    UniqueHandle<RenderHardwareInterface> rhi;
    BackendDescription description;
};

} // namespace vex