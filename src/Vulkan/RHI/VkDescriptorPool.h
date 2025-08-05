#pragma once

#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkRHIFwd.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolBase
{
public:
    VkDescriptorPool(NonNullPtr<VkGPUContext> ctx);
    virtual void CopyNullDescriptor(u32 slotIndex) override;

    void UpdateDescriptor(BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo, bool writeAccess);
    void UpdateDescriptor(BindlessHandle targetDescriptor,
                          ::vk::DescriptorType descType,
                          ::vk::DescriptorBufferInfo createInfo);

    // Updates which buffer is bound as the global bindlessMapping buffer.
    void UpdateBindlessMappingBuffer(RHIBuffer& bindlessMappingBuffer,
                                     ::vk::DeviceSize offset = 0,
                                     ::vk::DeviceSize range = VK_WHOLE_SIZE);

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;
    // For bindless resources (the actual place where the bindless resources are present).
    ::vk::UniqueDescriptorSet bindlessSet;
    ::vk::UniqueDescriptorSetLayout bindlessLayout;

    // For the buffer that contains the indices of the current pass's resources (aka the indices into the previous
    // set/layout).
    ::vk::UniqueDescriptorSetLayout bindlessMappingLayout;
    ::vk::UniqueDescriptorSet bindlessMappingSet;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk