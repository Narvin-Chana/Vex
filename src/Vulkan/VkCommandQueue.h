#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/Types.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkCommandQueue
{
    CommandQueueTypes::Value type = CommandQueueTypes::Invalid;
    u32 family = 0;
    ::vk::Queue queue;
};

} // namespace vex::vk
