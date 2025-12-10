#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12AccelerationStructure.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkAccelerationStructure.h>
#endif