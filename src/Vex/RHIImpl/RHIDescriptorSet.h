#pragma once

#if VEX_DX12
// TODO: Implement
// #include <DX12/RHI/DX12DescriptorPool.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkDescriptorSet.h>
#endif