#pragma once

#if VEX_DX12
#include <DX12/DX12RHIFwd.h>
#elif VEX_VULKAN
#include <Vulkan/VkRHIFwd.h>
#else
#error At least one GraphicsAPI must be valid!
#endif
