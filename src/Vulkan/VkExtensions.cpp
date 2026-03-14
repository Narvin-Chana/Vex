#include "VkExtensions.h"

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

bool SupportsExtension(const std::vector<::vk::ExtensionProperties>& extensionProperties,
                       std::string_view extensionName)
{
    for (const ::vk::ExtensionProperties& extensionProperty : extensionProperties)
    {
        if (std::string_view(extensionProperty.extensionName) == extensionName)
        {
            return true;
        }
    }

    return false;
}

} // namespace vex::vk
