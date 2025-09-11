#include "VkDescriptorSet.h"

#include "RHI/RHIDescriptorPool.h"
#include "Vex/Formattable.h"
#include "Vex/PhysicalDevice.h"
#include "Vulkan/VkDebug.h"
#include "Vulkan/VkErrorHandler.h"
#include "Vulkan/VkGPUContext.h"

namespace vex::vk
{
static constexpr ::vk::DescriptorBufferInfo NullDescriptorBufferInfo{ .buffer = VK_NULL_HANDLE,
                                                                      .offset = 0,
                                                                      .range = VK_WHOLE_SIZE };

static ::vk::DescriptorType DescriptorTypeToVulkanDescriptorType(DescriptorType type)
{
    using enum ::vk::DescriptorType;
    switch (type)
    {
    case DescriptorType::Buffer:
        return eUniformBuffer;
    case DescriptorType::RWBuffer:
        return eStorageBuffer;
    case DescriptorType::Sampler:
        return eSampler;
    case DescriptorType::Texture:
        return eSampledImage;
    case DescriptorType::RWTexture:
        return eStorageImage;
    default:
        VEX_LOG(Fatal, "Descriptor type ({}) not supported in vulkan", type)
    }
    std::unreachable();
}

VkDescriptorSet::VkDescriptorSet(NonNullPtr<VkGPUContext> ctx,
                                 const ::vk::DescriptorPool& descriptorPool,
                                 std::span<DescriptorType> descriptorTypes)
{
    std::vector<::vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(descriptorTypes.size());

    for (u32 i = 0; i < descriptorTypes.size(); ++i)
    {
        bindings.push_back(::vk::DescriptorSetLayoutBinding{
            .binding = i,
            .descriptorType = DescriptorTypeToVulkanDescriptorType(descriptorTypes[i]),
            .descriptorCount = 1,
            .stageFlags = ::vk::ShaderStageFlagBits::eAll,
            .pImmutableSamplers = nullptr,
        });
    }

    ::vk::DescriptorSetLayoutCreateInfo createInfo = {
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };

    // Create layout
    descriptorLayout = VEX_VK_CHECK <<= ctx->device.createDescriptorSetLayoutUnique(createInfo);

    ::vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*descriptorLayout,
    };

    std::vector<::vk::UniqueDescriptorSet> descSets = VEX_VK_CHECK <<=
        ctx->device.allocateDescriptorSetsUnique(descriptorSetAllocateInfo);

    descriptorSet = std::move(descSets[0]);

    SetDebugName(ctx->device, *descriptorSet, "Bindful Descriptor Set");
}

VkBindlessDescriptorSet::VkBindlessDescriptorSet(NonNullPtr<VkGPUContext> ctx,
                                                 const ::vk::DescriptorPool& descriptorPool)
    : ctx{ ctx }
{
    // Create a mutable descriptor binding set, this allows us to use the ResourceDescriptorHeap in HLSL shaders
    // which greatly simplifies the resulting code. It is important to know that this also disallows certain
    // aspects:
    //
    // - Bindless descriptors all belong to the same heap, which means bindless handle indices no longer have to
    // store the type of the pool.
    //
    // - To cite the Vulkan docs:
    // "A mutable descriptor is expected to consume as much memory as the largest descriptor type
    // it supports, and it is expected that there will be holes in GPU memory between descriptors when smaller
    // descriptor types are used. Using mutable descriptor types should only be considered when it is meaningful,
    // e.g. when the alternative is emitting 6+ large descriptor arrays as a workaround in bindless DirectX 12
    // emulation or similar. Using mutable descriptor types as a lazy workaround for using concrete descriptor types
    // will likely lead to lower GPU performance. It might also disable certain fast-paths in implementations since
    // the descriptors types are no longer statically known at layout creation time."
    //
    // I believe this trade off is worth it given it greatly simplifies our code.
    using enum ::vk::DescriptorType;
    std::vector descriptorTypes{ eSampler,       eSampledImage, eStorageImage, eUniformTexelBuffer, eStorageTexelBuffer,
                                 eUniformBuffer, eStorageBuffer };
    if (GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing))
    {
        descriptorTypes.push_back(eAccelerationStructureKHR);
    }

    ::vk::MutableDescriptorTypeListEXT mutableDescriptorTypeList = {
        .descriptorTypeCount = static_cast<u32>(descriptorTypes.size()),
        .pDescriptorTypes = descriptorTypes.data(),
    };

    ::vk::MutableDescriptorTypeCreateInfoEXT mutableTypeInfo = {
        .pNext = nullptr,
        .mutableDescriptorTypeListCount = 1,
        .pMutableDescriptorTypeLists = &mutableDescriptorTypeList,
    };

    ::vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding = {
        .binding = 0,
        .descriptorType = eMutableEXT,
        .descriptorCount = GDefaultDescriptorPoolSize,
        .stageFlags = ::vk::ShaderStageFlagBits::eAll,
        .pImmutableSamplers = nullptr,
    };

    ::vk::DescriptorBindingFlagsEXT bindingFlags =
        ::vk::DescriptorBindingFlagBits::ePartiallyBound | ::vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    ::vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo = {
        .pNext = &mutableTypeInfo,
        .bindingCount = 1,
        .pBindingFlags = &bindingFlags,
    };
    ::vk::DescriptorSetLayoutCreateInfo createInfo = {
        .pNext = &bindingFlagsInfo,
        .flags = ::vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding,
    };

    // Create layout
    descriptorLayout = VEX_VK_CHECK <<= ctx->device.createDescriptorSetLayoutUnique(createInfo);

    ::vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*descriptorLayout,
    };

    std::vector<::vk::UniqueDescriptorSet> descSets = VEX_VK_CHECK <<=
        ctx->device.allocateDescriptorSetsUnique(descriptorSetAllocateInfo);

    descriptorSet = std::move(descSets[0]);

    SetDebugName(ctx->device, *descriptorSet, "Bindless Descriptor Set");
}

void VkBindlessDescriptorSet::UpdateDescriptor(BindlessHandle targetDescriptor,
                                               ::vk::DescriptorImageInfo createInfo,
                                               bool hasGPUWriteAccess)
{
    ::vk::DescriptorType type =
        hasGPUWriteAccess ? ::vk::DescriptorType::eStorageImage : ::vk::DescriptorType::eSampledImage;
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &createInfo,
    };

    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkBindlessDescriptorSet::UpdateDescriptor(BindlessHandle targetDescriptor,
                                               ::vk::DescriptorType descType,
                                               ::vk::DescriptorBufferInfo createInfo)
{
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = descType,
        .pBufferInfo = &createInfo,
    };

    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkBindlessDescriptorSet::CopyNullDescriptor(u32 slotIndex)
{
    // We copy in any arbitrary null descriptor, in this case its a null buffer.
    const ::vk::WriteDescriptorSet nullWriteDescriptorSet{
        .dstSet = *descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = slotIndex,
        .descriptorCount = 1,
        .descriptorType = ::vk::DescriptorType::eStorageBuffer,
        .pImageInfo = nullptr,
        .pBufferInfo = &NullDescriptorBufferInfo,
        .pTexelBufferView = nullptr,
    };
    ctx->device.updateDescriptorSets(1, &nullWriteDescriptorSet, 0, nullptr);
}

} // namespace vex::vk