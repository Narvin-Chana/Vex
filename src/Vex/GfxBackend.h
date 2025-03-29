#pragma once

#include <Vex/Formats.h>
#include <Vex/PlatformWindow.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

enum class GraphicsAPI : u8
{
    DirectX12,
    Vulkan
};

struct BackendDescription
{
    GraphicsAPI api;
    PlatformWindow platformWindow;
    TextureFormat swapChainFormat;
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