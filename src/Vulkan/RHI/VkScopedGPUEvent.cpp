#include "VkScopedGPUEvent.h"

#include <Vulkan/RHI/VkCommandList.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

VkScopedGPUEvent::VkScopedGPUEvent(NonNullPtr<VkCommandList> commandList, const char* label, std::array<float, 3> color)
    : RHIScopedGPUEventBase(commandList, label, color)
{
    if (GEnableGPUScopedEvents)
    {
        std::array<float, 4> fullColor{ color[0], color[1], color[2], 1 };
        commandList->GetNativeCommandList().beginDebugUtilsLabelEXT(::vk::DebugUtilsLabelEXT{
            .pLabelName = label,
            .color = fullColor,
        });
    }
}

VkScopedGPUEvent::~VkScopedGPUEvent()
{
    if (emitMarker && GEnableGPUScopedEvents)
    {
        commandList->GetNativeCommandList().endDebugUtilsLabelEXT();
    }
}

} // namespace vex::vk
