#include "VkDebug.h"

#include <Vex/Logger.h>

namespace vex::vk
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(::vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             ::vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                             const ::vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData)
{

    if (messageSeverity >= ::vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        VEX_LOG(vex::LogLevel::Info, "validation layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

} // namespace vex::vk
