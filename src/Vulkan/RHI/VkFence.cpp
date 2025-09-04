#include "VkFence.h"

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

namespace VkFence_Internal
{

static ::vk::UniqueSemaphore CreateTimelineSemaphore(::vk::Device device)
{
    ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eTimeline, .initialValue = 0 };
    return VEX_VK_CHECK <<= device.createSemaphoreUnique(::vk::SemaphoreCreateInfo{
               .pNext = &createInfo,
           });
}

} // namespace VkFence_Internal

VkFence::VkFence(::vk::Device device)
    : timelineSemaphore(VkFence_Internal::CreateTimelineSemaphore(device))
    , device(device)
{
}

u64 VkFence::GetValue() const
{
    return VEX_VK_CHECK <<= device.getSemaphoreCounterValue(*timelineSemaphore);
}

void VkFence::WaitOnCPU(u64 value) const
{
    ::vk::SemaphoreWaitInfo waitInfo = {
        .flags = ::vk::SemaphoreWaitFlags{},
        .semaphoreCount = 1,
        .pSemaphores = &*timelineSemaphore,
        .pValues = &value,
    };

    // Wait indefinitely for the semaphore to reach the specified value
    VEX_VK_CHECK << device.waitSemaphores(&waitInfo, std::numeric_limits<u64>::max());
}

void VkFence::SignalOnCPU(u64 value)
{
    const ::vk::SemaphoreSignalInfo signalInfo = {
        .semaphore = *timelineSemaphore,
        .value = value,
    };

    VEX_VK_CHECK << device.signalSemaphore(&signalInfo);
}

} // namespace vex::vk