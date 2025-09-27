#pragma once

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(::vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             ::vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                             const ::vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* pUserData);

// Debug name can only be enabled when GPU debugging is active.
inline bool GEnableDebugName = false;

#if !VEX_SHIPPING
// Only pass Vk types
template <class T>
void SetDebugName(::vk::Device device, const T& object, const char* name)
{
    if (!GEnableDebugName)
    {
        return;
    }

    ::vk::DebugUtilsObjectNameInfoEXT debugNameInfo{
        .objectType = T::objectType,
        .objectHandle = reinterpret_cast<uint64_t>(typename T::NativeType(object)),
        .pObjectName = name,
    };
    VEX_VK_CHECK << device.setDebugUtilsObjectNameEXT(debugNameInfo);
}
#else
template <class T>
void SetDebugName(::vk::Device device, const T& object, const char* name) {};
#endif

} // namespace vex::vk