#include "Vex.h"

#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHI.h>

namespace vex
{

UniqueHandle<GfxBackend> CreateGraphicsBackend(const GraphicsCreateDesc& desc)
{
    if (desc.platformWindow.width == 0 || desc.platformWindow.height == 0)
    {
        VEX_LOG(Fatal,
                "Unable to create a window with width {} and height {}.",
                desc.platformWindow.width,
                desc.platformWindow.height);
        return nullptr;
    }
    VEX_LOG(Info, "Creating Graphics Backend with API Support:\n\tDX12: {} Vulkan: {}", VEX_DX12, VEX_VULKAN);

    return MakeUnique<GfxBackend>(desc);
}

} // namespace vex