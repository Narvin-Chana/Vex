#pragma once
#include "VkHeaders.h"

namespace vex::vk
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(::vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             ::vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                             const ::vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData);

}