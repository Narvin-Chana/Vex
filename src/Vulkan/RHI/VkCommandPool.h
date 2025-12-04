#pragma once

#include <Vex/QueueType.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHICommandPool.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkGPUContext;

class VkCommandPool final : public RHICommandPoolBase
{
public:
    VkCommandPool(RHI& rhi,
                  NonNullPtr<VkGPUContext> ctx,
                  const std::array<VkCommandQueue, QueueTypes::Count>& commandQueues);
    ~VkCommandPool();

    VkCommandPool(VkCommandPool&&) = default;
    VkCommandPool& operator=(VkCommandPool&&) = default;

    virtual NonNullPtr<RHICommandList> GetOrCreateCommandList(QueueType queueType) override;

private:
    ::vk::UniqueCommandPool& GetCommandPool(QueueType queueType);

    NonNullPtr<VkGPUContext> ctx;

    std::array<::vk::UniqueCommandPool, QueueTypes::Count> commandPoolPerQueue;
};

} // namespace vex::vk