#pragma once

#include <Vex/RHI/RHIFence.h>

#include "Vulkan/VkHeaders.h"

namespace vex::vk
{
class VkFence final : public RHIFence
{
public:
    VkFence(u32 numFenceIndices, ::vk::Device device);

    u64 GetCompletedFenceValue() override;

protected:
    void WaitCPU_Internal(u32 index) override;
    ::vk::UniqueSemaphore fence;
    ::vk::Device device;

    friend class VkRHI;
};
} // namespace vex::vk