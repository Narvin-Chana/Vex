#pragma once
#include <RHI/RHITimestampQueryPool.h>

namespace vex::vk
{

class VkTimestampQueryPool final : public RHITimestampQueryPoolBase
{
    ::vk::UniqueQueryPool queryPool;

    NonNullPtr<VkGPUContext> ctx;

protected:
    double GetTimestampPeriod(QueueType type) override;

public:
    VkTimestampQueryPool(NonNullPtr<VkGPUContext> ctx, RHI& rhi, RHIAllocator& allocator);

    ::vk::QueryPool GetNativeQueryPool() const noexcept
    {
        return *queryPool;
    }

    friend VkCommandList;
};

} // namespace vex::vk
