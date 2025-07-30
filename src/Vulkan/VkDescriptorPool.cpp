#include "VkDescriptorPool.h"

#include <Vex/Debug.h>
#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHITexture.h>
#include <Vex/UniqueHandle.h>

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{
static constexpr u32 BindlessMaxDescriptorPerType = 1000;
static constexpr ::vk::DescriptorImageInfo NullDescriptorImageInfo{ .sampler = nullptr,
                                                                    .imageView = nullptr,
                                                                    .imageLayout = ::vk::ImageLayout::eUndefined };

static constexpr ::vk::DescriptorBufferInfo NullDescriptorBufferInfo{ .buffer = nullptr, .offset = 0, .range = 0 };

VkDescriptorPool::VkDescriptorPool(::vk::Device device)
    : device{ device }
{
    std::vector<::vk::DescriptorPoolSize> poolSize;
    poolSize.reserve(DescriptorTypes.size());
    std::ranges::transform(
        DescriptorTypes,
        std::back_inserter(poolSize),
        [](auto type)
        { return ::vk::DescriptorPoolSize{ .type = type, .descriptorCount = BindlessMaxDescriptorPerType }; });

    ::vk::DescriptorPoolCreateInfo descriptorPoolInfo{
        .flags = ::vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 ::vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
        .pPoolSizes = poolSize.data(),
    };

    descriptorPool = VEX_VK_CHECK <<= device.createDescriptorPoolUnique(descriptorPoolInfo);

    // Inspired from https://dev.to/gasim/implementing-bindless-design-in-vulkan-34no
    std::array<::vk::DescriptorSetLayoutBinding, DescriptorTypes.size()> bindings{};
    std::array<::vk::DescriptorBindingFlags, DescriptorTypes.size()> flags{};

    for (uint32_t i = 0; i < DescriptorTypes.size(); ++i)
    {
        bindings[i].binding = i;
        bindings[i].descriptorType = DescriptorTypes[i];
        bindings[i].descriptorCount = BindlessMaxDescriptorPerType;
        bindings[i].stageFlags = ::vk::ShaderStageFlagBits::eAll;
        flags[i] = ::vk::DescriptorBindingFlagBits::ePartiallyBound | ::vk::DescriptorBindingFlagBits::eUpdateAfterBind;
    }

    ::vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.pBindingFlags = flags.data();
    bindingFlags.bindingCount = DescriptorTypes.size();

    ::vk::DescriptorSetLayoutCreateInfo createInfo{};
    createInfo.bindingCount = DescriptorTypes.size();
    createInfo.pBindings = bindings.data();
    createInfo.flags = ::vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    createInfo.pNext = &bindingFlags;

    // Create layout
    bindlessLayout = VEX_VK_CHECK <<= device.createDescriptorSetLayoutUnique(createInfo);

    ::vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*bindlessLayout,
    };

    std::vector<::vk::UniqueDescriptorSet> descSets = VEX_VK_CHECK <<=
        device.allocateDescriptorSetsUnique(descriptorSetAllocateInfo);

    bindlessSet = std::move(descSets[0]);

    for (int i = 0; i < DescriptorTypes.size(); i++)
    {
        bindlessAllocations[i].generations.resize(BindlessMaxDescriptorPerType);
        bindlessAllocations[i].handles = FreeListAllocator(BindlessMaxDescriptorPerType);
    }

    // The pool should be split into two sections, one for static descriptors (aka resources that we load in once
    // at startup or for streaming and reuse thereafter) and one section for dynamic descriptors (for resources that
    // are created and destroyed during the same frame, eg: temporary buffers).
}

VkDescriptorPool::~VkDescriptorPool() = default;

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHITexture& texture, bool writeAccess)
{
    auto type = writeAccess ? ::vk::DescriptorType::eStorageImage : ::vk::DescriptorType::eSampledImage;

    BindlessAllocation& alloc = bindlessAllocations[GetDescriptorTypeBinding(type)];

    u32 index = alloc.handles.Allocate();

    return BindlessHandle::CreateHandle(index, alloc.generations[index], type);
}

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHIBuffer& buffer)
{
    // This handles both StructuredBuffer AND RWStructuredBuffer
    auto type = ::vk::DescriptorType::eStorageBuffer;

    BindlessAllocation& alloc = bindlessAllocations[GetDescriptorTypeBinding(type)];
    u32 index = alloc.handles.Allocate();

    return BindlessHandle::CreateHandle(index, alloc.generations[index], type);
}

void VkDescriptorPool::FreeStaticDescriptor(BindlessHandle handle)
{
    const ::vk::DescriptorType type = GetDescriptorTypeFromHandle(handle);
    const ::vk::DescriptorImageInfo* imageInfo{};
    const ::vk::DescriptorBufferInfo* bufferInfo{};

    switch (type)
    {
    case ::vk::DescriptorType::eSampledImage:
    case ::vk::DescriptorType::eStorageImage:
        imageInfo = &NullDescriptorImageInfo;
        break;
    case ::vk::DescriptorType::eStorageBuffer:
    case ::vk::DescriptorType::eUniformBuffer:
        bufferInfo = &NullDescriptorBufferInfo;
        break;
    default:
        VEX_ASSERT(false, "Bindless handle type not supported");
    }

    const u32 index = handle.GetIndex();

    const ::vk::WriteDescriptorSet NullWriteDescriptorSet{ .dstSet = *bindlessSet,
                                                           .dstBinding = GetDescriptorTypeBinding(type),
                                                           .dstArrayElement = index,
                                                           .descriptorCount = 1,
                                                           .descriptorType = type,
                                                           .pImageInfo = imageInfo,
                                                           .pBufferInfo = bufferInfo,
                                                           .pTexelBufferView = nullptr };

    device.updateDescriptorSets(1, &NullWriteDescriptorSet, 0, nullptr);

    auto& [generations, handles] = GetAllocation(handle);
    generations[index]++;
    handles.Deallocate(index);
}

BindlessHandle VkDescriptorPool::AllocateDynamicDescriptor(const RHITexture& texture)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

BindlessHandle VkDescriptorPool::AllocateDynamicDescriptor(const RHIBuffer& buffer)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void VkDescriptorPool::FreeDynamicDescriptor(BindlessHandle handle)
{
    VEX_NOT_YET_IMPLEMENTED();
}

bool VkDescriptorPool::IsValid(BindlessHandle handle)
{
    return handle.GetGeneration() == GetAllocation(handle).generations[handle.GetIndex()];
}

u8 VkDescriptorPool::GetDescriptorTypeBinding(BindlessHandle handle)
{
    auto handleType = GetDescriptorTypeFromHandle(handle);
    return GetDescriptorTypeBinding(handleType);
}

void VkDescriptorPool::UpdateDescriptor(VkGPUContext& ctx,
                                        BindlessHandle targetDescriptor,
                                        ::vk::DescriptorImageInfo createInfo)
{
    auto descType = GetDescriptorTypeFromHandle(targetDescriptor);
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *bindlessSet,
        .dstBinding = GetDescriptorTypeBinding(descType),
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = descType,
        .pImageInfo = &createInfo,
    };

    ctx.device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}
void VkDescriptorPool::UpdateDescriptor(VkGPUContext& ctx,
                                        BindlessHandle targetDescriptor,
                                        ::vk::DescriptorBufferInfo createInfo)
{
    auto descType = GetDescriptorTypeFromHandle(targetDescriptor);
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *bindlessSet,
        .dstBinding = GetDescriptorTypeBinding(descType),
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = descType,
        .pBufferInfo = &createInfo,
    };

    ctx.device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

VkDescriptorPool::BindlessAllocation& VkDescriptorPool::GetAllocation(BindlessHandle handle)
{
    return bindlessAllocations[GetDescriptorTypeBinding(handle)];
}

VkDescriptorPool::BindlessAllocation& VkDescriptorPool::GetAllocation(::vk::DescriptorType type)
{
    return bindlessAllocations[GetDescriptorTypeBinding(type)];
}

::vk::DescriptorType VkDescriptorPool::GetDescriptorTypeFromHandle(BindlessHandle handle)
{
    return handle.type;
}
u8 VkDescriptorPool::GetDescriptorTypeBinding(::vk::DescriptorType type)
{
    for (int i = 0; i < DescriptorTypes.size(); ++i)
    {
        if (DescriptorTypes[i] == type)
            return i;
    }

    VEX_LOG(Fatal, "Bindless handle not found in metadata");
    std::unreachable();
}

} // namespace vex::vk