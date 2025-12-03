#pragma once

#include <Vex/PhysicalDevice.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkPhysicalDevice : public PhysicalDevice
{
    VkPhysicalDevice(const ::vk::PhysicalDevice& dev);
    ~VkPhysicalDevice() = default;

    VkPhysicalDevice(const VkPhysicalDevice&) = delete;
    VkPhysicalDevice& operator=(const VkPhysicalDevice&) = delete;

    VkPhysicalDevice(VkPhysicalDevice&&) = default;
    VkPhysicalDevice& operator=(VkPhysicalDevice&&) = default;

    static double GetDeviceVRAMSize(const ::vk::PhysicalDevice& physicalDevice);

    ::vk::PhysicalDevice physicalDevice;
};

} // namespace vex::vk