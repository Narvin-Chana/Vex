#include <Vulkan/RHI/VkScopedGPUEvent.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

VkScopedGPUEvent::VkScopedGPUEvent(::vk::CommandBuffer buffer, const char* label, std::array<float, 3> color)
    : RHIScopedGPUEventBase(label, color)
    , buffer{ buffer }
{
    std::array<float, 4> fullColor{ color[0], color[1], color[2], 1 };
    buffer.beginDebugUtilsLabelEXT(::vk::DebugUtilsLabelEXT{
        .pLabelName = label,
        .color = fullColor,
    });
}

VkScopedGPUEvent::~VkScopedGPUEvent()
{
    if (emitMarker)
    {
        buffer.endDebugUtilsLabelEXT();
    }
}

} // namespace vex::vk
