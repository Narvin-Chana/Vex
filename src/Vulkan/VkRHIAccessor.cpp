#include "VkRHIAccessor.h"

namespace vex::vk
{

VkRHIAccessor::~VkRHIAccessor()
{
}

void* VkRHIAccessor::GetNativeDevice()
{
    return nullptr;
}

void* VkRHIAccessor::GetNativeDescriptorPool()
{
    return nullptr;
}

} // namespace vex::vk