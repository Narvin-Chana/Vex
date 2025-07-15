#include "VkFence.h"

#include <Vulkan/VkErrorHandler.h>

namespace vex::vk
{

VkFence::VkFence(u32 numFenceIndices, ::vk::Device device)
    : RHIFence(numFenceIndices)
    , device{ device }
{
    ::vk::SemaphoreTypeCreateInfoKHR type{ .semaphoreType = ::vk::SemaphoreType::eTimeline,
                                           .initialValue = GetFenceValue(0) };
    ++GetFenceValue(0);

    fence = VEX_VK_CHECK <<= device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
        .pNext = &type,
    });
}

u64 VkFence::GetCompletedFenceValue()
{
    u64 value;
    VEX_VK_CHECK << device.getSemaphoreCounterValue(*fence, &value);
    return value;
}

void VkFence::WaitCPU_Internal(u32 index)
{
    ::vk::SemaphoreWaitInfoKHR waitInfo{ .semaphoreCount = 1,
                                         .pSemaphores = &*fence,
                                         .pValues = &GetFenceValue(index) };

    VEX_VK_CHECK << device.waitSemaphoresKHR(&waitInfo, std::numeric_limits<u64>::max());
}

} // namespace vex::vk
