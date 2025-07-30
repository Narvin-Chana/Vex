#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12DescriptorPool.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkDescriptorPool.h>
#endif