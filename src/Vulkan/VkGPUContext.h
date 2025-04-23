#pragma once

#include "VkHeaders.h"

namespace vex::vk
{
struct VkCommandQueue;
}
namespace vex::vk
{

struct VkGPUContext
{
    ::vk::Device device;
    ::vk::PhysicalDevice physDevice;
    ::vk::SurfaceKHR surface;

    VkCommandQueue& graphicsPresentQueue;
};

} // namespace vex::vk
