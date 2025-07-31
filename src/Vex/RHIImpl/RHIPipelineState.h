#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12PipelineState.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VkPipelineState.h>
#endif