#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12Fence.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkFence.h>
#endif