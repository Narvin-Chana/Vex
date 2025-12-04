#pragma once

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/DrawHelpers.h>
#include <Vex/FeatureChecker.h>
#include <Vex/Graphics.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/TextureSampler.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/UniqueHandle.h>
#include <Vex/Utility/Formattable.h>

namespace vex
{
enum class GraphicsAPI : u8
{
    DirectX12,
    Vulkan
};

} // namespace vex
