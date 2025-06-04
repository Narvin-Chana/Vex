#include "VkDescriptorPool.h"

#include <Vex/Debug.h>

namespace vex::vk
{

VkDescriptorPool::VkDescriptorPool(::vk::Device device)
{
    // Initialize descriptor pool at a certain (large) default size.
    // For now we should ignore resizing as it is a big hassle and instead allocate an arbitrarily large pool.
    //
    // The pool should be split into two sections, one for static descriptors (aka resources that we load in once
    // at startup or for streaming and reuse thereafter) and one section for dynamic descriptors (for resources that
    // are created and destroyed during the same frame, eg: temporary buffers).
    VEX_NOT_YET_IMPLEMENTED();
}

VkDescriptorPool::~VkDescriptorPool() = default;

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHITexture& texture)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHIBuffer& buffer)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void VkDescriptorPool::FreeStaticDescriptor(BindlessHandle handle)
{
    VEX_NOT_YET_IMPLEMENTED();
}

BindlessHandle VkDescriptorPool::AllocateDynamicDescriptor(const RHITexture& texture)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

BindlessHandle VkDescriptorPool::AllocateDynamicDescriptor(const RHIBuffer& buffer)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void VkDescriptorPool::FreeDynamicDescriptor(BindlessHandle handle)
{
    VEX_NOT_YET_IMPLEMENTED();
}

bool VkDescriptorPool::IsValid(BindlessHandle handle)
{
    VEX_NOT_YET_IMPLEMENTED();
    return false;
}

} // namespace vex::vk