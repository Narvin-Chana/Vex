#pragma once

#include <Vex/PhysicalDevice.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkPhysicalDevice : public PhysicalDevice
{
    ::vk::PhysicalDevice physicalDevice;
};

} // namespace vex::vk