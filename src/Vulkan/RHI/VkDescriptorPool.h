#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/RHIFwd.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolInterface
{
public:
    VkDescriptorPool(::vk::Device device);

    BindlessHandle AllocateStaticDescriptor();
    void FreeStaticDescriptor(BindlessHandle handle);

    BindlessHandle AllocateDynamicDescriptor();
    void FreeDynamicDescriptor(BindlessHandle handle);

    bool IsValid(BindlessHandle handle);

    void UpdateDescriptor(VkGPUContext& ctx,
                          BindlessHandle targetDescriptor,
                          ::vk::DescriptorImageInfo createInfo,
                          bool writeAccess);
    void UpdateDescriptor(VkGPUContext& ctx, BindlessHandle targetDescriptor, ::vk::DescriptorBufferInfo createInfo);

private:
    ::vk::Device device;
    ::vk::UniqueDescriptorPool descriptorPool;
    ::vk::UniqueDescriptorSet bindlessSet; // Single global set for bindless resources
    ::vk::UniqueDescriptorSetLayout bindlessLayout;

    struct BindlessAllocation
    {
        std::vector<u8> generations;
        FreeListAllocator handles;
    };
    BindlessAllocation bindlessAllocation;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk