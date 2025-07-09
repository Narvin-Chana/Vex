#pragma once

#include <Vex/RHI/RHIDescriptorPool.h>

#include "VkHeaders.h"

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool : public RHIDescriptorPool
{
public:
    VkDescriptorPool(::vk::Device device);
    virtual ~VkDescriptorPool() override;

    virtual BindlessHandle AllocateStaticDescriptor(const RHITexture& texture) override;
    virtual BindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer) override;
    virtual void FreeStaticDescriptor(BindlessHandle handle) override;

    virtual BindlessHandle AllocateDynamicDescriptor(const RHITexture& texture) override;
    virtual BindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer) override;
    virtual void FreeDynamicDescriptor(BindlessHandle handle) override;

    virtual bool IsValid(BindlessHandle handle) override;

    void UpdateDescriptor(VkGPUContext& ctx, BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo);

private:
    static constexpr std::array DescriptorTypes{
        ::vk::DescriptorType::eUniformBuffer,
        ::vk::DescriptorType::eStorageBuffer,
        ::vk::DescriptorType::eSampledImage,
        ::vk::DescriptorType::eStorageImage,
    };

    ::vk::Device device;
    ::vk::UniqueDescriptorPool descriptorPool;
    ::vk::UniqueDescriptorSet bindlessSet; // Single global set for bindless resources
    ::vk::UniqueDescriptorSetLayout bindlessLayout;

    struct BindlessAllocation
    {
        std::vector<u8> generations;
        FreeListAllocator handles;
    };
    std::array<BindlessAllocation, DescriptorTypes.size()> bindlessAllocations;
    BindlessAllocation& GetAllocation(BindlessHandle::Type handle);
    BindlessAllocation& GetAllocation(BindlessHandle handle);

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk