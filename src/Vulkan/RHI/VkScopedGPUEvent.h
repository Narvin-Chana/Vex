#pragma once
#include <array>

#include <RHI/RHIScopedGPUEvent.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkScopedGPUEvent final : public RHIScopedGPUEventBase
{
public:
    VkScopedGPUEvent(::vk::CommandBuffer queue, const char* label, std::array<float, 3> color);
    VkScopedGPUEvent(VkScopedGPUEvent&&) = default;
    VkScopedGPUEvent& operator=(VkScopedGPUEvent&&) = default;
    ~VkScopedGPUEvent();

private:
    ::vk::CommandBuffer buffer;
};

} // namespace vex::vk