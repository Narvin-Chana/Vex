#pragma once

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/DrawHelpers.h>
#include <Vex/FeatureChecker.h>
#include <Vex/GfxBackend.h>

namespace vex
{
enum class GraphicsAPI : u8
{
    DirectX12,
    Vulkan
};

UniqueHandle<GfxBackend> CreateGraphicsBackend(const BackendDescription& description);

} // namespace vex
