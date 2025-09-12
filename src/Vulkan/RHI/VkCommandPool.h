#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/NonNullPtr.h>
#include <Vex/UniqueHandle.h>

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
                  const std::array<VkCommandQueue, CommandQueueTypes::Count>& commandQueues);
    ~VkCommandPool();

    VkCommandPool(VkCommandPool&&) = default;
    VkCommandPool& operator=(VkCommandPool&&) = default;

    virtual NonNullPtr<RHICommandList> GetOrCreateCommandList(CommandQueueType queueType) override;

private:
    ::vk::UniqueCommandPool& GetCommandPool(CommandQueueType queueType);

    NonNullPtr<VkGPUContext> ctx;

    std::array<::vk::UniqueCommandPool, CommandQueueTypes::Count> commandPoolPerQueue;
};

} // namespace vex::vk