#include <Vulkan/RHI/VkScopedDebugMarker.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

VkScopedDebugMarker::VkScopedDebugMarker(::vk::CommandBuffer buffer, const char* label, std::array<float, 3> color)
    : RHIScopedDebugMarkerBase(label, color)
    , buffer{ buffer }
{
    std::array<float, 4> fullColor{ color[0], color[2], color[2], 1 };
    buffer.beginDebugUtilsLabelEXT(::vk::DebugUtilsLabelEXT{
        .pLabelName = label,
        .color = fullColor,
    });
}

VkScopedDebugMarker::~VkScopedDebugMarker()
{
    if (emitMarker)
    {
        buffer.endDebugUtilsLabelEXT();
    }
}

} // namespace vex::vk
