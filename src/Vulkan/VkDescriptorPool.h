#pragma once

#include <Vex/RHI/RHIDescriptorPool.h>

#include "VkHeaders.h"

namespace vex::vk
{

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

private:
    ::vk::DescriptorPool descriptorPool;
    ::vk::DescriptorSet bindlessSet; // Single global set for bindless resources
    ::vk::DescriptorSetLayout bindlessLayout;
};

} // namespace vex::vk