#include "VkPhysicalDevice.h"

namespace vex::vk
{

VkPhysicalDevice::VkPhysicalDevice(const ::vk::PhysicalDevice& dev)
    : physicalDevice(dev)
{
    info.deviceName = dev.getProperties().deviceName.data();
    info.dedicatedVideoMemoryMB = GetDeviceVRAMSize(dev);
    featureChecker.emplace(dev);
}

double VkPhysicalDevice::GetDeviceVRAMSize(const ::vk::PhysicalDevice& physicalDevice)
{
    ::vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    double totalDeviceLocalMemoryMB = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        if (memoryProperties.memoryHeaps[i].flags & ::vk::MemoryHeapFlagBits::eDeviceLocal)
        {
            // Device local implies its VRAM memory
            VkDeviceSize heapSize = memoryProperties.memoryHeaps[i].size;

            // Convert to a more readable format (MB)
            totalDeviceLocalMemoryMB += static_cast<double>(heapSize) / (1024.0 * 1024.0);
        }
    }
    return totalDeviceLocalMemoryMB;
}

} // namespace vex::vk