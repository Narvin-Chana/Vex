#pragma once

#include <array>

#include <RHI/RHIFwd.h>
#include <RHI/RHIScopedGPUEvent.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkScopedGPUEvent final : public RHIScopedGPUEventBase
{
public:
    VkScopedGPUEvent(NonNullPtr<VkCommandList> commandList, const char* label, std::array<float, 3> color);
    VkScopedGPUEvent(VkScopedGPUEvent&&) = default;
    VkScopedGPUEvent& operator=(VkScopedGPUEvent&&) = default;
    ~VkScopedGPUEvent();
};

} // namespace vex::vk