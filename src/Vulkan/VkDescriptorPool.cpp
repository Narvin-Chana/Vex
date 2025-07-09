#include "VkDescriptorPool.h"

#include "Vex/UniqueHandle.h"
#include "VkErrorHandler.h"
#include "VkGPUContext.h"

#include <Vex/Debug.h>

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
        .flags = ::vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
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

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHITexture& texture)
{
    BindlessAllocation& alloc = bindlessAllocations[std::to_underlying(BindlessHandle::Type::Texture)];

    u32 index = alloc.handles.Allocate();

    return BindlessHandle::CreateHandle(index, alloc.generations[index], BindlessHandle::Type::Texture);
}

BindlessHandle VkDescriptorPool::AllocateStaticDescriptor(const RHIBuffer& buffer)
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void VkDescriptorPool::FreeStaticDescriptor(BindlessHandle handle)
{
    ::vk::DescriptorType type{};
    const ::vk::DescriptorImageInfo* imageInfo{};
    const ::vk::DescriptorBufferInfo* bufferInfo{};
    switch (handle.type)
    {
    case BindlessHandle::Type::Texture:
        type = ::vk::DescriptorType::eCombinedImageSampler;
        imageInfo = &NullDescriptorImageInfo;
        break;
    case BindlessHandle::Type::StorageBuffer:
        type = ::vk::DescriptorType::eStorageBuffer;
        bufferInfo = &NullDescriptorBufferInfo;
        break;
    case BindlessHandle::Type::UniformBuffer:
        type = ::vk::DescriptorType::eUniformBuffer;
        bufferInfo = &NullDescriptorBufferInfo;
        break;
    default:

        VEX_ASSERT(false, "Bindless handle type not supported");
    }

    const u32 index = handle.GetIndex();

    const ::vk::WriteDescriptorSet NullWriteDescriptorSet{ .dstSet = *bindlessSet,
                                                           .dstBinding = index,
                                                           .dstArrayElement = 0, // check if this makes sense?
                                                           .descriptorCount = 1,
                                                           .descriptorType = type,
                                                           .pImageInfo = imageInfo,
                                                           .pBufferInfo = bufferInfo,
                                                           .pTexelBufferView = nullptr };

    device.updateDescriptorSets(1, &NullWriteDescriptorSet, 0, nullptr);

    auto& [generations, handles] = bindlessAllocations[std::to_underlying(handle.type)];
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
    return handle.GetGeneration() ==
           bindlessAllocations[std::to_underlying(handle.type)].generations[handle.GetIndex()];
}

void VkDescriptorPool::UpdateDescriptor(VkGPUContext& ctx,
                                        BindlessHandle targetDescriptor,
                                        ::vk::DescriptorImageInfo createInfo)
{
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *bindlessSet,
        .dstBinding = 3,
        .dstArrayElement = targetDescriptor.value,
        .descriptorCount = 1,
        .descriptorType = ::vk::DescriptorType::eStorageImage,
        .pImageInfo = &createInfo,
    };

    ctx.device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

VkDescriptorPool::BindlessAllocation& VkDescriptorPool::GetAllocation(BindlessHandle::Type type)
{
    return bindlessAllocations[std::to_underlying(type)];
}
VkDescriptorPool::BindlessAllocation& VkDescriptorPool::GetAllocation(BindlessHandle handle)
{
    return GetAllocation(handle.type);
}

} // namespace vex::vk