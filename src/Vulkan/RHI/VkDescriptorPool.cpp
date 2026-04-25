#include "VkDescriptorPool.h"

#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkSampler.h>

namespace vex::vk
{

VkDescriptorPool::VkDescriptorPool(NonNullPtr<VkGPUContext> ctx)
    : ctx{ ctx }
{
    std::array poolSize{ ::vk::DescriptorPoolSize{
                             .type = ::vk::DescriptorType::eMutableEXT,
                             .descriptorCount = GDefaultDescriptorPoolSize,
                         },
                         ::vk::DescriptorPoolSize{
                             .type = ::vk::DescriptorType::eSampler,
                             .descriptorCount = GDefaultDescriptorPoolSize,
                         },
                         ::vk::DescriptorPoolSize{
                             .type = ::vk::DescriptorType::eCombinedImageSampler,
                             .descriptorCount = GDefaultDescriptorPoolSize,
                         } };

    ::vk::DescriptorPoolCreateInfo descriptorPoolInfo{
        .flags = ::vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 ::vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets =
            1024, // External users might need to create more descriptor sets than the bindless one (ImGui for example)
        .poolSizeCount = static_cast<u32>(poolSize.size()),
        .pPoolSizes = poolSize.data(),
    };

    descriptorPool = VEX_VK_CHECK <<= ctx->device.createDescriptorPoolUnique(descriptorPoolInfo);

    bindlessSet.emplace(ctx, *descriptorPool);
}

BindlessHandle VkDescriptorPool::CreateBindlessSampler(const BindlessTextureSampler& textureSampler)
{
    // In Vk sampler descriptors can be allocated in the same heap as resource descriptors.
    const BindlessHandle handle = AllocateStaticDescriptor(DescriptorType::Resource);

    ::vk::SamplerCustomBorderColorCreateInfoEXT customBorder;
    const ::vk::SamplerCreateInfo samplerCI =
        GraphicsPipeline::GetVkSamplerCreateInfoFromBindlessTextureSampler(textureSampler, customBorder);
    ::vk::UniqueSampler vkSampler = VEX_VK_CHECK <<= ctx->device.createSamplerUnique(samplerCI);

    const ::vk::DescriptorImageInfo imageInfo{ .sampler = *vkSampler };
    bindlessSet->UpdateDescriptor(handle, imageInfo, ::vk::DescriptorType::eSampler);

    // Have to keep the sampler alive.
    samplers[handle] = std::move(vkSampler);

    return handle;
}

void VkDescriptorPool::FreeBindlessSampler(BindlessHandle handle)
{
    // TODO(https://trello.com/c/T1DY4QOT): Samplers lifespan has to be correctly managed on the GPU timeline.
    samplers.erase(handle);
    FreeStaticDescriptor(DescriptorType::Resource, handle);
}

void VkDescriptorPool::CopyNullDescriptor(DescriptorType, u32 slotIndex)
{
    // Vk can ignore descriptorType, since one heap can contain both samplers and resources.
    bindlessSet->SetDescriptorToNull(slotIndex);
}

VkBindlessDescriptorSet& VkDescriptorPool::GetBindlessSet()
{
    return *bindlessSet;
}

} // namespace vex::vk