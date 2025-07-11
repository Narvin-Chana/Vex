#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/RHI/RHIDescriptorPool.h>

#include "VkHeaders.h"

namespace vex::vk
{
struct VkGPUContext;

struct VkBindlessHandle : Handle<VkBindlessHandle>
{
    ::vk::DescriptorType type = static_cast<::vk::DescriptorType>(~0);

    static HandleType CreateHandle(u32 index, u8 generation, ::vk::DescriptorType type)
    {
        HandleType handle = Handle::CreateHandle(index, generation);
        handle.type = type;
        return handle;
    }
};

static constexpr VkBindlessHandle GInvalidVkBindlessHandle;

class VkDescriptorPool : public RHIDescriptorPool
{
public:
    VkDescriptorPool(::vk::Device device);
    virtual ~VkDescriptorPool() override;

    VkBindlessHandle AllocateStaticDescriptor(const RHITexture& texture);
    VkBindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer);
    void FreeStaticDescriptor(VkBindlessHandle handle);

    VkBindlessHandle AllocateDynamicDescriptor(const RHITexture& texture);
    VkBindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer);
    void FreeDynamicDescriptor(VkBindlessHandle handle);

    bool IsValid(VkBindlessHandle handle);

    void UpdateDescriptor(VkGPUContext& ctx, VkBindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo);

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

    BindlessAllocation& GetAllocation(VkBindlessHandle handle);
    BindlessAllocation& GetAllocation(::vk::DescriptorType type);

    ::vk::DescriptorType GetDescriptorTypeFromHandle(VkBindlessHandle handle);
    u8 GetDescriptorTypeBinding(::vk::DescriptorType type);
    u8 GetDescriptorTypeBinding(VkBindlessHandle handle);

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk