#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12Allocator.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkAllocator.h>
#endif