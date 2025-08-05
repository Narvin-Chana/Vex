#include "VkDescriptorPool.h"

#include <Vex/Debug.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/UniqueHandle.h>

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

static constexpr ::vk::DescriptorBufferInfo NullDescriptorBufferInfo{ .buffer = VK_NULL_HANDLE,
                                                                      .offset = 0,
                                                                      .range = VK_WHOLE_SIZE };

VkDescriptorPool::VkDescriptorPool(NonNullPtr<VkGPUContext> ctx)
    : ctx{ ctx }
{
    ::vk::DescriptorPoolSize poolSize{
        .type = ::vk::DescriptorType::eMutableEXT,
        .descriptorCount = DefaultPoolSize,
    };

    ::vk::DescriptorPoolSize uniformPoolSize{
        .type = ::vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    };

    std::array poolSizes = { poolSize, uniformPoolSize };

    ::vk::DescriptorPoolCreateInfo descriptorPoolInfo{
        .flags = ::vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 ::vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 2,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };

    descriptorPool = VEX_VK_CHECK <<= ctx->device.createDescriptorPoolUnique(descriptorPoolInfo);

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
    {
        std::vector<::vk::DescriptorType> descriptorTypes = {
            ::vk::DescriptorType::eSampledImage,       ::vk::DescriptorType::eStorageImage,
            ::vk::DescriptorType::eUniformTexelBuffer, ::vk::DescriptorType::eStorageTexelBuffer,
            ::vk::DescriptorType::eUniformBuffer,      ::vk::DescriptorType::eStorageBuffer,
        };
        if (GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing))
        {
            descriptorTypes.push_back(::vk::DescriptorType::eAccelerationStructureKHR);
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
            .descriptorType = ::vk::DescriptorType::eMutableEXT,
            .descriptorCount = DefaultPoolSize,
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
        bindlessLayout = VEX_VK_CHECK <<= ctx->device.createDescriptorSetLayoutUnique(createInfo);

        ::vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{
            .descriptorPool = *descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*bindlessLayout,
        };

        std::vector<::vk::UniqueDescriptorSet> descSets = VEX_VK_CHECK <<=
            ctx->device.allocateDescriptorSetsUnique(descriptorSetAllocateInfo);

        bindlessSet = std::move(descSets[0]);
    }

    // TODO: The pool should be split into two sections, one for static descriptors (aka resources that we load in once
    // at startup or for streaming and reuse thereafter) and one section for dynamic descriptors (for resources that
    // are created and destroyed during the same frame, eg: temporary buffers).

    // Create the bindlessMappingLayout and bindlessMappingSet.
    // These are used to have a bound constant buffer slot who's purpose is to contain all global resources the user
    // uses during the pass. This allows for them to still be used in a bindless manner via shader codegen, reducing
    // CPU work as much as possible.
    {
        ::vk::DescriptorSetLayoutBinding binding = {
            .binding = 0, // binding 0 in space 1
            .descriptorType = ::vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = ::vk::ShaderStageFlagBits::eAll,
        };
        ::vk::DescriptorBindingFlagsEXT bindingFlags = ::vk::DescriptorBindingFlagBits::eUpdateAfterBind;
        ::vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo = {
            .bindingCount = 1,
            .pBindingFlags = &bindingFlags,
        };
        ::vk::DescriptorSetLayoutCreateInfo layoutInfo = {
            .pNext = &bindingFlagsInfo,
            .flags = ::vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = 1,
            .pBindings = &binding,
        };
        bindlessMappingLayout = VEX_VK_CHECK <<= ctx->device.createDescriptorSetLayoutUnique(layoutInfo);

        ::vk::DescriptorSetAllocateInfo allocInfo = {
            .descriptorPool = *descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*bindlessMappingLayout,
        };
        std::vector<::vk::UniqueDescriptorSet> descSets = VEX_VK_CHECK <<=
            ctx->device.allocateDescriptorSetsUnique(allocInfo);

        bindlessMappingSet = std::move(descSets[0]);
    }
}

void VkDescriptorPool::CopyNullDescriptor(u32 slotIndex)
{
    // We copy in any arbitrary null descriptor, in this case its a null buffer.
    const ::vk::WriteDescriptorSet nullWriteDescriptorSet{
        .dstSet = *bindlessSet,
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

void VkDescriptorPool::UpdateDescriptor(BindlessHandle targetDescriptor,
                                        ::vk::DescriptorImageInfo createInfo,
                                        bool hasGPUWriteAccess)
{
    auto type = hasGPUWriteAccess ? ::vk::DescriptorType::eStorageImage : ::vk::DescriptorType::eSampledImage;
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *bindlessSet,
        .dstBinding = 0,
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &createInfo,
    };

    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkDescriptorPool::UpdateDescriptor(BindlessHandle targetDescriptor,
                                        ::vk::DescriptorType descType,
                                        ::vk::DescriptorBufferInfo createInfo)
{
    const ::vk::WriteDescriptorSet writeSet{
        .dstSet = *bindlessSet,
        .dstBinding = 0,
        .dstArrayElement = targetDescriptor.GetIndex(),
        .descriptorCount = 1,
        .descriptorType = descType,
        .pBufferInfo = &createInfo,
    };

    ctx->device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void VkDescriptorPool::UpdateBindlessMappingBuffer(RHIBuffer& bindlessMappingBuffer,
                                                   ::vk::DeviceSize offset,
                                                   ::vk::DeviceSize range)
{
    ::vk::DescriptorBufferInfo bufferInfo = {
        .buffer = bindlessMappingBuffer.GetNativeBuffer(),
        .offset = offset,
        .range = range,
    };

    ::vk::WriteDescriptorSet writeDescriptorSet = {
        .dstSet = *bindlessMappingSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = ::vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &bufferInfo,
    };

    ctx->device.updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

} // namespace vex::vk