#include "VkResourceLayout.h"

#include <Vex/Debug.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIBuffer.h>

#include <RHI/RHIDescriptorPool.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{
VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool)
    : ctx{ ctx }
    , descriptorPool{ descriptorPool }
{
}
const RHIDescriptorSet& VkResourceLayout::GetResourceLayoutDescriptor()
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
                                   .size = GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize() };

    std::vector<DescriptorType> descriptorTypes;
    descriptorTypes.resize(samplers.size());
    std::fill_n(descriptorTypes.begin(), samplers.size(), DescriptorType::Sampler);
    samplerSet = VkDescriptorSet(ctx, *descriptorPool->descriptorPool, descriptorTypes);

    std::array layouts = { *samplerSet->descriptorLayout, *descriptorPool->bindlessSet->descriptorLayout };
    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = static_cast<u32>(layouts.size()),
                                               .pSetLayouts = layouts.data(),
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    ::vk::UniquePipelineLayout pipelineLayout = VEX_VK_CHECK <<= ctx->device.createPipelineLayoutUnique(createInfo);

    std::vector<::vk::WriteDescriptorSet> writeSets;
    writeSets.reserve(samplers.size());

    vkSamplers.clear();
    for (u32 i = 0; i < samplers.size(); ++i)
    {
        const TextureSampler& sampler = samplers[i];
        ::vk::SamplerCreateInfo samplerCI{
            // TODO: fill sampler data
        };

        ::vk::UniqueSampler vkSampler = VEX_VK_CHECK <<= ctx->device.createSamplerUnique(samplerCI);

        ::vk::DescriptorImageInfo imageInfo{
            .sampler = *vkSampler,
        };

        writeSets.push_back(::vk::WriteDescriptorSet{
            .dstSet = *samplerSet->descriptorSet,
            .dstBinding = i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = ::vk::DescriptorType::eSampler,
            .pImageInfo = &imageInfo,
        });

        vkSamplers.push_back(std::move(vkSampler));
    }

    // upload samplers to descriptor
    ctx->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);

    version++;

    return pipelineLayout;
}

} // namespace vex::vk