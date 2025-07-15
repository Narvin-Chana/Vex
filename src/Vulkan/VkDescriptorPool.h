#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/RHI/RHIDescriptorPool.h>

#include "Vex/Handle.h"
#include "VkHeaders.h"

namespace vex::vk
{
struct VkGPUContext;

struct BindlessHandle : Handle<BindlessHandle>
{
    ::vk::DescriptorType type = static_cast<::vk::DescriptorType>(~0);

    static BindlessHandle CreateHandle(u32 index, u8 generation, ::vk::DescriptorType type)
    {
        BindlessHandle handle = Handle::CreateHandle(index, generation);
        handle.type = type;
        return handle;
    }
};

static constexpr BindlessHandle GInvalidBindlessHandle;

class VkDescriptorPool : public RHIDescriptorPool
{
public:
    VkDescriptorPool(::vk::Device device);
    virtual ~VkDescriptorPool() override;

    BindlessHandle AllocateStaticDescriptor(const RHITexture& texture, bool writeAccess);
    BindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer);
    void FreeStaticDescriptor(BindlessHandle handle);

    BindlessHandle AllocateDynamicDescriptor(const RHITexture& texture);
    BindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer);
    void FreeDynamicDescriptor(BindlessHandle handle);

    bool IsValid(BindlessHandle handle);

    void UpdateDescriptor(VkGPUContext& ctx, BindlessHandle targetDescriptor, ::vk::DescriptorImageInfo createInfo);
    void UpdateDescriptor(VkGPUContext& ctx, BindlessHandle targetDescriptor, ::vk::DescriptorBufferInfo createInfo);

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

    BindlessAllocation& GetAllocation(BindlessHandle handle);
    BindlessAllocation& GetAllocation(::vk::DescriptorType type);

    ::vk::DescriptorType GetDescriptorTypeFromHandle(BindlessHandle handle);
    u8 GetDescriptorTypeBinding(::vk::DescriptorType type);
    u8 GetDescriptorTypeBinding(BindlessHandle handle);

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk