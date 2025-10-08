#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12ScopedDebugMarker.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkScopedDebugMarker.h>
#endif