#pragma once
#include <variant>

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Resource.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkDescriptorPool;
struct VkGPUContext;

class VkDescriptorSet final
{
    VkDescriptorSet(NonNullPtr<VkGPUContext> ctx,
                    const ::vk::DescriptorPool& descriptorPool,
                    Span<::vk::DescriptorType> descriptorTypes);

public:
    void UpdateDescriptor(u32 index, ::vk::DescriptorImageInfo createInfo);
    void UpdateDescriptors(u32 startIndex, Span<const ::vk::DescriptorImageInfo> createInfo);
    void UpdateDescriptor(u32 index, ::vk::DescriptorBufferInfo createInfo);
    void UpdateDescriptors(u32 startIndex, Span<const ::vk::DescriptorBufferInfo> createInfo);

private:
    ::vk::UniqueDescriptorSet descriptorSet;
    ::vk::UniqueDescriptorSetLayout descriptorLayout;
    std::vector<::vk::DescriptorType> descriptorTypes;

    NonNullPtr<VkGPUContext> ctx;

    friend class VkDescriptorPool;
    friend class VkResourceLayout;
    friend class VkCommandList;
};

class VkBindlessDescriptorSet final
{
public:
    VkBindlessDescriptorSet(NonNullPtr<VkGPUContext> ctx, const ::vk::DescriptorPool& descriptorPool);
    void UpdateDescriptor(BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo, bool writeAccess);
    void UpdateDescriptor(BindlessHandle targetDescriptor,
                          ::vk::DescriptorType descType,
                          ::vk::DescriptorBufferInfo createInfo);
    void SetDescriptorToNull(u32 index);

    ::vk::UniqueDescriptorSet descriptorSet;
    ::vk::UniqueDescriptorSetLayout descriptorLayout;
    NonNullPtr<VkGPUContext> ctx;

    friend class VkDescriptorPool;
    friend class VkResourceLayout;
};

} // namespace vex::vk
