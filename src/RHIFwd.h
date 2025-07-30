#pragma once

#if VEX_DX12
#include <DX12/DX12RHIFwd.h>
#elif VEX_VULKAN
#include <Vulkan/VkRHIFwd.h>
#else
#error Must define at least one graphics API!
#endif