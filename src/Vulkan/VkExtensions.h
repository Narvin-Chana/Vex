#pragma once

#include <vector>

namespace vex::vk
{

std::vector<const char*> GetRequiredInstanceExtensions(bool enableGPUDebugLayer);
std::vector<const char*> GetDefaultDeviceExtensions();

std::vector<const char*> GetDefaultValidationLayers(bool enableGPUBasedValidation);
std::vector<const char*> FilterSupportedValidationLayers(const std::vector<const char*>& layers);

} // namespace vex::vk
