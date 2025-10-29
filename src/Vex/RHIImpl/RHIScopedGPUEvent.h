#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12ScopedGPUEvent.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkScopedGPUEvent.h>
#endif