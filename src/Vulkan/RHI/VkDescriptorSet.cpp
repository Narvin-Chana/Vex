#include "VkDescriptorSet.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/Validation.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{
static constexpr ::vk::DescriptorBufferInfo NullDescriptorBufferInfo{ .buffer = VK_NULL_HANDLE,
                                                                      .offset = 0,
                                                                      .range = VK_WHOLE_SIZE };

static void ValidateImageDescriptorType(::vk::DescriptorType type)
{
    VEX_CHECK(type == ::vk::DescriptorType::eStorageImage || type == ::vk::DescriptorType::eSampledImage ||
                  type == ::vk::DescriptorType::eSampler,
              "Tried to set descriptor of type {} with a Image descriptor info. Must be a Image or Sampler "
              "compatible one",
              type)
}
static void ValidateImageDescriptor(::vk::DescriptorType type, const ::vk::DescriptorImageInfo& createInfo)
{
    ValidateImageDescriptorType(type);

    VEX_CHECK(!(type == ::vk::DescriptorType::eSampler && !createInfo.sampler),
              "Tried to set descriptor of type Sampler with a Image descriptor info that doesnt have sampler set. "
              "Sampler must be set")
}

static void ValidateBufferDescriptor(::vk::DescriptorType type){
    VEX_CHECK(type == ::vk::DescriptorType::eUniformBuffer || type == ::vk::DescriptorType::eStorageBuffer,
              "Tried to set descriptor of type {} with a buffer descriptor info. Must be a buffer compatible one",
              type)
}

VkDescriptorSet::VkDescriptorSet(NonNullPtr<VkGPUContext> ctx,
                                 const ::vk::DescriptorPool& descriptorPool,
                                 Span<::vk::DescriptorType> descriptorTypes)
    : descriptorTypes{ descriptorTypes.begin(), descriptorTypes.end() }
    , ctx{ ctx }
{
    std::vector<::vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(descriptorTypes.size());

    for (u32 i = 0; i < descriptorTypes.size(); ++i)
    {
        bindings.push_back(::vk::DescriptorSetLayoutBinding{
            .binding = i,
            .descriptorType = descriptorTypes[i],
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

void VkDescriptorSet::UpdateDescriptor(u32 index, ::vk::DescriptorImageInfo createInfo)
{
    ::vk::DescriptorType type = descriptorTypes[index];

    ValidateImageDescriptor(type, createInfo);

    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &createInfo,
    };
    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkDescriptorSet::UpdateDescriptors(u32 startIndex, Span<const ::vk::DescriptorImageInfo> createInfos)
{
    ::vk::DescriptorType type = descriptorTypes[startIndex];
    ValidateImageDescriptorType(type);

    std::vector<::vk::WriteDescriptorSet> writeSets;
    writeSets.reserve(createInfos.size());

    for (u32 i = 0; i < createInfos.size(); ++i)
    {
        ValidateImageDescriptor(type, createInfos[i]);
        writeSets.push_back(::vk::WriteDescriptorSet{
            .dstSet = *descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = startIndex + i,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = &createInfos[i],
        });
    }

    ctx->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
}

void VkDescriptorSet::UpdateDescriptor(u32 index, ::vk::DescriptorBufferInfo createInfo)
{
    ::vk::DescriptorType type = descriptorTypes[index];
    ValidateBufferDescriptor(type);

    const ::vk::WriteDescriptorSet writeSet{ .dstSet = *descriptorSet,
                                             .dstBinding = 0,
                                             .dstArrayElement = index,
                                             .descriptorCount = 1,
                                             .descriptorType = type,
                                             .pBufferInfo = &createInfo };
    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkDescriptorSet::UpdateDescriptors(u32 startIndex, Span<const ::vk::DescriptorBufferInfo> createInfos)
{
    ::vk::DescriptorType type = descriptorTypes[startIndex];

    std::vector<::vk::WriteDescriptorSet> writeSets;
    writeSets.reserve(createInfos.size());

    for (u32 i = 0; i < createInfos.size(); ++i)
    {
        ValidateBufferDescriptor(descriptorTypes[i]);
        writeSets.push_back(::vk::WriteDescriptorSet{
            .dstSet = *descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = startIndex + i,
            .descriptorCount = 1,
            .descriptorType = type,
            .pBufferInfo = &createInfos[i],
        });
    }

    ctx->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
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
    if (GPhysicalDevice->IsFeatureSupported(Feature::RayTracing))
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

void VkBindlessDescriptorSet::SetDescriptorToNull(u32 index)
{
    // We copy in any arbitrary null descriptor, in this case its a null buffer.
    const ::vk::WriteDescriptorSet nullWriteDescriptorSet{
        .dstSet = *descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = ::vk::DescriptorType::eStorageBuffer,
        .pImageInfo = nullptr,
        .pBufferInfo = &NullDescriptorBufferInfo,
        .pTexelBufferView = nullptr,
    };
    ctx->device.updateDescriptorSets(1, &nullWriteDescriptorSet, 0, nullptr);
}

} // namespace vex::vk