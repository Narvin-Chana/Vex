#include "Vex.h"

#include <Vex/Logger.h>

#if VEX_DX12
#include <DX12/DX12RHI.h>
#endif
#if VEX_VULKAN
#include <Vulkan/VkRHI.h>
#endif

namespace vex
{

UniqueHandle<GfxBackend> CreateGraphicsBackend(const BackendDescription& description)
{
    if (description.platformWindow.width == 0 || description.platformWindow.height == 0)
    {
        VEX_LOG(Fatal,
                "Unable to create a window with width {} and height {}.",
                description.platformWindow.width,
                description.platformWindow.height);
        return {};
    }
    UniqueHandle<GfxBackend> backend;
#if VEX_DX12
    backend = MakeUnique<GfxBackend>(MakeUnique<vex::dx12::DX12RHI>(description.platformWindow.windowHandle,
                                                                    description.enableGPUDebugLayer,
                                                                    description.enableGPUBasedValidation),
                                     description);
#elif VEX_VULKAN
    backend = MakeUnique<GfxBackend>(MakeUnique<vex::vk::VkRHI>(description.platformWindow.windowHandle,
                                                                description.enableGPUDebugLayer,
                                                                description.enableGPUBasedValidation),
                                     description);
#else
#error No supported GraphicsAPI, check your build tool config.
#endif

    return backend;
}

} // namespace vex