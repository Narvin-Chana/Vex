#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12TimestampQueryPool.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkTimestampQueryPool.h>
#endif