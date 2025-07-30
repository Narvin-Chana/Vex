#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12CommandList.h>
#else VEX_VULKAN
#include <Vulkan/RHI/VkCommandList.h>
#endif