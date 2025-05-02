#pragma once

#include "Vex/Logger.h"
#include "VkHeaders.h"

namespace vex::vk
{
inline u32 GetBestMemoryType(::vk::PhysicalDevice device, u32 typeFilter, ::vk::MemoryPropertyFlags flags)
{
    ::vk::PhysicalDeviceMemoryProperties memProperties = device.getMemoryProperties();

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & flags) == flags)
        {
            return i;
        }
    }

    VEX_LOG(Fatal, "Unsuitable memory found for flags {:x}", static_cast<u32>(flags))
    return 0;
}
} // namespace vex::vk
