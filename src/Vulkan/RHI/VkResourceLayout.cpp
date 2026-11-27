#include "VkResourceLayout.h"

#include <Vex/PhysicalDevice.h>
#include <VexMacros.h>
#include <Vex/RHIImpl/RHIBuffer.h>

#include <Vulkan/RHI/VkDeferredRelease.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkSampler.h>

namespace vex::vk
{

VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool)
    : ctx{ ctx }
    , descriptorPool{ descriptorPool }
{
    Rebuild();
}

::vk::PipelineLayout VkResourceLayout::GetPipelineLayout() const
{
    return *pipelineLayout;
}

::vk::DescriptorSet VkResourceLayout::GetStaticSamplerDescriptorSet() const
{
    return *samplerSet->descriptorSet;
}

void VkResourceLayout::Cleanup(std::vector<RHIDeferredRelease>& deferredReleases)
{
    if (pipelineLayout)
    {
        deferredReleases.emplace_back(std::move(pipelineLayout));
    }

    for (::vk::UniqueSampler& vkSampler : vkSamplers)
    {
        deferredReleases.emplace_back(std::move(vkSampler));
    }
    vkSamplers.clear();

    if (samplerSet)
    {
        deferredReleases.emplace_back(std::move(samplerSet->descriptorSet));
        deferredReleases.emplace_back(std::move(samplerSet->descriptorLayout));
        samplerSet.reset();
    }
}

void VkResourceLayout::Compile()
{
    std::array<::vk::DescriptorType, MaxSamplerCount> descriptorTypes{};
    std::fill_n(descriptorTypes.begin(), staticSamplers.size(), ::vk::DescriptorType::eSampler);
    samplerSet = VkDescriptorSet(ctx, *descriptorPool->descriptorPool, descriptorTypes);

    pipelineLayout = CreateLayout();
}

::vk::ShaderStageFlags VkResourceLayout::GetPushConstantStageFlags()
{
    return ::vk::ShaderStageFlagBits::eAll;
}

::vk::UniquePipelineLayout VkResourceLayout::CreateLayout()
{
    ::vk::PushConstantRange range{ .stageFlags = GetPushConstantStageFlags(),
                                   .offset = 0,
                                   .size = GPhysicalDevice->GetMaxLocalConstantsByteSize() };

    std::array layouts = { *descriptorPool->GetBindlessSet().descriptorLayout, *samplerSet->descriptorLayout };
    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = static_cast<u32>(layouts.size()),
                                               .pSetLayouts = layouts.data(),
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    ::vk::UniquePipelineLayout vkPipelineLayout = VEX_VK_CHECK <<= ctx->device.createPipelineLayoutUnique(createInfo);

    std::vector<::vk::WriteDescriptorSet> writeSets;
    // Needed because writeSets stores a pointer to descriptorImageInfos
    std::vector<::vk::DescriptorImageInfo> descriptorImageInfos;
    writeSets.reserve(staticSamplers.size());
    descriptorImageInfos.reserve(staticSamplers.size());

    vkSamplers.clear();
    for (u32 i = 0; i < staticSamplers.size(); ++i)
    {
        const StaticTextureSampler& sampler = staticSamplers[i];
        ::vk::SamplerCreateInfo samplerCI = GraphicsPipeline::GetVkSamplerCreateInfoFromStaticTextureSampler(sampler);
        ::vk::UniqueSampler vkSampler = VEX_VK_CHECK <<= ctx->device.createSamplerUnique(samplerCI);

        descriptorImageInfos.emplace_back(*vkSampler);

        vkSamplers.push_back(std::move(vkSampler));
    }

    samplerSet->UpdateDescriptors(0, descriptorImageInfos);

    return vkPipelineLayout;
}

} // namespace vex::vk