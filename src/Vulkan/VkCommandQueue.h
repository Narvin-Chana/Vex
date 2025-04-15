#pragma once

#include <Vex/RHI/RHI.h>
#include <Vex/Types.h>

#include "VkHeaders.h"

namespace vex::vk
{

struct VkCommandQueue
{
    CommandQueueTypes::Value type;
    u32 family = 0;
    ::vk::Queue queue;
};

} // namespace vex::vk
