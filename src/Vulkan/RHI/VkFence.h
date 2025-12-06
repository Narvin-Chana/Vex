#pragma once

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFence.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

struct VkGPUContext;

class VkFence : public RHIFenceBase
{
public:
    VkFence(::vk::Device device);
    virtual u64 GetValue() const override;
    virtual void WaitOnCPU(u64 value) const override;
    virtual void SignalOnCPU(u64 value) override;

    ::vk::UniqueSemaphore timelineSemaphore;

private:
    ::vk::Device device;
};

} // namespace vex::vk