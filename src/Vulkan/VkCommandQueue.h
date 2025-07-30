#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/Types.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkCommandQueue
{
    CommandQueueTypes::Value type;
    u32 family = 0;
    ::vk::Queue queue;

    // Value to wait on when submitting commands. Should signal waitValue + 1 after every command waiting on this
    u32 waitValue;
    // Timeline semaphore to wait on
    ::vk::UniqueSemaphore waitSemaphore;
};

} // namespace vex::vk
