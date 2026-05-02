#pragma once

#include <unordered_map>

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/RHI/VkDescriptorSet.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolBase
{
public:
    VkDescriptorPool(NonNullPtr<VkGPUContext> ctx);
    virtual BindlessHandle CreateBindlessSampler(const BindlessTextureSampler& textureSampler) override;
    virtual void FreeBindlessSampler(BindlessHandle handle) override;

    ::vk::DescriptorPool& GetNativeDescriptorPool()
    {
        return *descriptorPool;
    }
    virtual void CopyNullDescriptor(DescriptorType descriptorType, u32 slotIndex) override;

    VkBindlessDescriptorSet& GetBindlessSet();

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;

    MaybeUninitialized<VkBindlessDescriptorSet> bindlessSet;

    std::unordered_map<BindlessHandle, ::vk::UniqueSampler> samplers;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk