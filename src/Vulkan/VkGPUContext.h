#pragma once

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkCommandQueue;

struct VkGPUContext
{
    ::vk::Device device;
    ::vk::PhysicalDevice physDevice;
    ::vk::SurfaceKHR surface;

    VkCommandQueue& graphicsPresentQueue;
};

} // namespace vex::vk
