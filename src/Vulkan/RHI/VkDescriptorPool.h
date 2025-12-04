#pragma once

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIDescriptorPool.h>
#include <RHI/RHIFwd.h>

#include <Vulkan/RHI/VkDescriptorSet.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolBase
{
public:
    VkDescriptorPool(NonNullPtr<VkGPUContext> ctx);

    ::vk::DescriptorPool& GetNativeDescriptorPool()
    {
        return *descriptorPool;
    }
    virtual void CopyNullDescriptor(u32 slotIndex) override;

    VkBindlessDescriptorSet& GetBindlessSet();

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;

    MaybeUninitialized<VkBindlessDescriptorSet> bindlessSet;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk