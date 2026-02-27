#pragma once

#include <string_view>
#include <vector>

namespace vk
{
struct ExtensionProperties;
}

namespace vex::vk
{

bool SupportsExtension(const std::vector<::vk::ExtensionProperties>& extensionProperties,
                       std::string_view extensionName);

} // namespace vex::vk
