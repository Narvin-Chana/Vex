#pragma once

#include <Vex/QueueType.h>
#include <Vex/Types.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkCommandQueue
{
    QueueTypes::Value type = QueueTypes::Invalid;
    u32 family = 0;
    ::vk::Queue queue;
};

} // namespace vex::vk
