#include "Vex/Logger.h"
#include "VkDebug.h"

namespace vex::vk
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             ::vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                             const ::vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData)
{

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        VEX_LOG(vex::LogLevel::Info, "Validation layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

} // namespace vex::vk
