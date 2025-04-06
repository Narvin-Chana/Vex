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

UniqueHandle<GfxBackend> CreateGraphicsBackend(GraphicsAPI api, const BackendDescription& description)
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
    switch (api)
    {
#if VEX_DX12
    case GraphicsAPI::DirectX12:
        backend = MakeUnique<GfxBackend>(
            MakeUnique<vex::dx12::DX12RHI>(description.enableGPUDebugLayer, description.enableGPUBasedValidation),
            description);
        break;
#endif
#if VEX_VULKAN
    case GraphicsAPI::Vulkan:
        backend = MakeUnique<GfxBackend>(MakeUnique<vex::vk::VkRHI>(description.platformWindow.windowHandle,
                                                                    description.enableGPUDebugLayer,
                                                                    description.enableGPUBasedValidation),
                                         description);
        break;
#endif
    default:
        VEX_LOG(Fatal, "Attempting to create a graphics backend for an unsupported API.");
    }

    return backend;
}

} // namespace vex