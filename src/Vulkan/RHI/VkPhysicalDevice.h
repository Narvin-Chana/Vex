#pragma once

#include <RHI/RHIPhysicalDevice.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkPhysicalDevice : public RHIPhysicalDeviceBase
{
public:
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