#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12Buffer.h>
#else VEX_VULKAN
#include <Vulkan/RHI/VkBuffer.h>
#endif