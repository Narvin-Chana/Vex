#pragma once
#include <array>

#include <RHI/RHIScopedDebugMarker.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkScopedDebugMarker final : public RHIScopedDebugMarkerBase
{
public:
    VkScopedDebugMarker(::vk::CommandBuffer queue, const char* label, std::array<float, 3> color);
    VkScopedDebugMarker(VkScopedDebugMarker&&) = default;
    VkScopedDebugMarker& operator=(VkScopedDebugMarker&&) = default;
    ~VkScopedDebugMarker();

private:
    ::vk::CommandBuffer buffer;
};

} // namespace vex::vk