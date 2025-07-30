#pragma once

#if VEX_DX12
#include <DX12/DX12RHITypes.h>
#elif VEX_VULKAN
#include <Vulkan/VkRHITypes.h>
#else
#error Must define at least one graphics API!
#endif