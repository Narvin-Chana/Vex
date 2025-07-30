#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12SwapChain.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkSwapChain.h>
#endif