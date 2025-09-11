#pragma once
#include <Vex/NonNullPtr.h>

#include <Vulkan/VkHeaders.h>

#include "RHI/RHIDescriptorSet.h"
#include "Vex/Resource.h"

namespace vex
{
enum class DescriptorType : u8;
}

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorSet final : public RHIDescriptorSetBase
{
    VkDescriptorSet(NonNullPtr<VkGPUContext> ctx,
                    const ::vk::DescriptorPool& descriptorPool,
                    std::span<DescriptorType> descriptorTypes);

public:
    ::vk::UniqueDescriptorSet descriptorSet;
    ::vk::UniqueDescriptorSetLayout descriptorLayout;

    friend class VkDescriptorPool;
    friend class VkResourceLayout;
};

class VkBindlessDescriptorSet final : public RHIBindlessDescriptorSetBase
{
public:
    VkBindlessDescriptorSet(NonNullPtr<VkGPUContext> ctx, const ::vk::DescriptorPool& descriptorPool);
    void UpdateDescriptor(BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo, bool writeAccess);
    void UpdateDescriptor(BindlessHandle targetDescriptor,
                          ::vk::DescriptorType descType,
                          ::vk::DescriptorBufferInfo createInfo);
    virtual void CopyNullDescriptor(u32 slotIndex) override;
    ::vk::UniqueDescriptorSet descriptorSet;
    ::vk::UniqueDescriptorSetLayout descriptorLayout;
    NonNullPtr<VkGPUContext> ctx;

    friend class VkDescriptorPool;
    friend class VkResourceLayout;
};

} // namespace vex::vk
