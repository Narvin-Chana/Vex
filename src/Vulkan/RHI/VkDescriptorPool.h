#pragma once

#include <Vex/NonNullPtr.h>

#include <RHI/RHIDescriptorPool.h>
#include <RHI/RHIFwd.h>

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

    ::vk::DescriptorPool& GetNativeDescriptorPool()
    {
        return *descriptorPool;
    }

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;
    // For bindless resources.
    ::vk::UniqueDescriptorSet bindlessSet;
    ::vk::UniqueDescriptorSetLayout bindlessLayout;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk