#pragma once

#include <Vex/PhysicalDevice.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkPhysicalDevice : public PhysicalDevice
{
    VkPhysicalDevice(const ::vk::PhysicalDevice& dev);
    static double GetDeviceVRAMSize(const ::vk::PhysicalDevice& physicalDevice);

    ::vk::PhysicalDevice physicalDevice;
};

} // namespace vex::vk