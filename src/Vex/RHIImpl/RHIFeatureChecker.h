#pragma once

#if VEX_DX12
#include <DX12/RHI/DX12FeatureChecker.h>
#elif VEX_VULKAN
#include <Vulkan/RHI/VKFeatureChecker.h>
#endif