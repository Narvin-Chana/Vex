#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12DescriptorSet.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkDescriptorSet.h>
#endif