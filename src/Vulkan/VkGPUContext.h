#pragma once

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkCommandQueue;
struct VkFence;

struct VkGPUContext
{
    ::vk::Device device;
    ::vk::PhysicalDevice physDevice;
    ::vk::SurfaceKHR surface;

    VkCommandQueue& graphicsPresentQueue;

    std::vector<u32> queueFamilyIndices;
};

} // namespace vex::vk
