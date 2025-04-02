#pragma once
#include <vector>

namespace vex::vk
{

std::vector<const char*> GetDefaultInstanceExtensions();
std::vector<const char*> GetDefaultDeviceExtensions();

std::vector<const char*> GetDefaultValidationLayers();
std::vector<const char*> FilterSupportedValidationLayers(const std::vector<const char*>& layers);

} // namespace vex::vk
