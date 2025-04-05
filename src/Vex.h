#pragma once

#include <Vex/FeatureChecker.h>
#include <Vex/GfxBackend.h>

namespace vex
{
enum class GraphicsAPI : u8
{
    DirectX12,
    Vulkan
};

UniqueHandle<GfxBackend> CreateGraphicsBackend(GraphicsAPI api, const BackendDescription& description);

} // namespace vex
