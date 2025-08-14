#pragma once

#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolInterface
{
public:
    VkDescriptorPool(NonNullPtr<VkGPUContext> ctx);
    virtual void CopyNullDescriptor(u32 slotIndex) override;

    void UpdateDescriptor(BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo, bool writeAccess);
    void UpdateDescriptor(BindlessHandle targetDescriptor,
                          ::vk::DescriptorType descType,
                          ::vk::DescriptorBufferInfo createInfo);

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;
    ::vk::UniqueDescriptorSet bindlessSet; // Single global set for bindless resources
    ::vk::UniqueDescriptorSetLayout bindlessLayout;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk