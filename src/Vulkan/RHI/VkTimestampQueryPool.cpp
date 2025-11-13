#include "VkTimestampQueryPool.h"

#include <Vulkan/RHI/VkCommandList.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

double VkTimestampQueryPool::GetTimestampPeriod(QueueType type)
{
    // timestampPeriod is in ns/tick
    return ctx->physDevice.getProperties().limits.timestampPeriod * 1000000000;
}

VkTimestampQueryPool::VkTimestampQueryPool(NonNullPtr<VkGPUContext> ctx, RHI& rhi, RHIAllocator& allocator)
    : RHITimestampQueryPoolBase(rhi, allocator)
    , ctx{ ctx }
{
    queryPool = VEX_VK_CHECK <<= ctx->device.createQueryPoolUnique({
        .queryType = ::vk::QueryType::eTimestamp,
        .queryCount = MaxInFlightQueriesCount * 2,
    });
}

} // namespace vex::vk
