#include "VkDebug.h"

#include <Vex/Logger.h>

namespace vex::vk
{

static VkBool32 debugCallbackCpp(::vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

// GCC does not compile with the VulkanHpp types in this signature.
// This is a quick fix.
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT messageType,
                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData)
{
    return debugCallbackCpp(static_cast<::vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity),
                            static_cast<::vk::DebugUtilsMessageTypeFlagsEXT>(messageType),
                            reinterpret_cast<const ::vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData),
                            pUserData);
}

} // namespace vex::vk
