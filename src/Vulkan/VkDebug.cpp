#include "VkDebug.h"

#include <Vex/Logger.h>

namespace vex::vk
{

static LogLevel GetLogLevelFromMessageSeverity(::vk::DebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity)
    {
    case ::vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        return LogLevel::Verbose;
    case ::vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        return LogLevel::Info;
    case ::vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        return LogLevel::Warning;
    case ::vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        return LogLevel::Fatal;
    default:
        return LogLevel::Fatal;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(::vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             ::vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                             const ::vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData)
{
    // TODO(https://trello.com/c/OnAJiNGP): This callback is not being called for errors
    LogLevel logLevel = GetLogLevelFromMessageSeverity(messageSeverity);
    if (logLevel >= LogLevel::Warning)
    {
        VEX_LOG(logLevel, "validation layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

} // namespace vex::vk
