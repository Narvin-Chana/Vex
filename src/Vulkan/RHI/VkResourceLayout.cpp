#include "VkResourceLayout.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Debug.h>
#include <Vex/RHIImpl/RHIBuffer.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkSamplers.h>

namespace vex::vk
{

VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool)
    : ctx{ ctx }
    , descriptorPool{ descriptorPool }
{
    std::array<::vk::DescriptorType, MaxSamplerCount> descriptorTypes{};
    std::fill_n(descriptorTypes.begin(), samplers.size(), ::vk::DescriptorType::eSampler);
    samplerSet = VkDescriptorSet(ctx, *descriptorPool->descriptorPool, descriptorTypes);
}

const VkDescriptorSet& VkResourceLayout::GetSamplerDescriptor()
{
    if (isDirty)
    {
        pipelineLayout = CreateLayout();
        isDirty = false;
    }
    return *samplerSet;
}

::vk::UniquePipelineLayout VkResourceLayout::CreateLayout()
{
    ::vk::PushConstantRange range{ .stageFlags =
                                       ::vk::ShaderStageFlagBits::eAllGraphics | ::vk::ShaderStageFlagBits::eCompute,
                                   .offset = 0,
                                   .size = GPhysicalDevice->GetMaxLocalConstantsByteSize() };

    std::array layouts = { *samplerSet->descriptorLayout, *descriptorPool->GetBindlessSet().descriptorLayout };
    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = static_cast<u32>(layouts.size()),
                                               .pSetLayouts = layouts.data(),
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    ::vk::UniquePipelineLayout pipelineLayout = VEX_VK_CHECK <<= ctx->device.createPipelineLayoutUnique(createInfo);

    std::vector<::vk::WriteDescriptorSet> writeSets;
    // Needed because writeSets stores a pointer to descriptorImageInfos
    std::vector<::vk::DescriptorImageInfo> descriptorImageInfos;
    writeSets.reserve(samplers.size());
    descriptorImageInfos.reserve(samplers.size());

    vkSamplers.clear();
    for (u32 i = 0; i < samplers.size(); ++i)
    {
        const TextureSampler& sampler = samplers[i];

        bool useAnisotropy = sampler.minFilter == FilterMode::Anisotropic ||
                             sampler.magFilter == FilterMode::Anisotropic ||
                             sampler.mipFilter == FilterMode::Anisotropic;

        ::vk::SamplerCreateInfo samplerCI{ .magFilter = FilterModeToVkFilter(sampler.magFilter),
                                           .minFilter = FilterModeToVkFilter(sampler.minFilter),
                                           .mipmapMode = FilterModeToVkMipMapMode(sampler.mipFilter),
                                           .addressModeU = AddressModeToVkSamplerAddressMode(sampler.addressU),
                                           .addressModeV = AddressModeToVkSamplerAddressMode(sampler.addressV),
                                           .addressModeW = AddressModeToVkSamplerAddressMode(sampler.addressW),
                                           .mipLodBias = sampler.mipLODBias,
                                           .anisotropyEnable = useAnisotropy,
                                           .maxAnisotropy = static_cast<float>(sampler.maxAnisotropy),
                                           .compareEnable = sampler.compareOp != CompareOp::Never,
                                           .compareOp = static_cast<::vk::CompareOp>(sampler.compareOp),
                                           .minLod = sampler.minLOD,
                                           .maxLod = sampler.maxLOD,
                                           .borderColor = BorderColorToVkBorderColor(sampler.borderColor),
                                           .unnormalizedCoordinates = false };

        ::vk::UniqueSampler vkSampler = VEX_VK_CHECK <<= ctx->device.createSamplerUnique(samplerCI);

        descriptorImageInfos.emplace_back(*vkSampler);

        vkSamplers.push_back(std::move(vkSampler));
    }

    samplerSet->UpdateDescriptors(0, descriptorImageInfos);

    version++;

    return pipelineLayout;
}

} // namespace vex::vk