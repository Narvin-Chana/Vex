#include "VkResourceLayout.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <VexMacros.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkSampler.h>

namespace vex::vk
{

VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx,
                                   NonNullPtr<VkDescriptorPool> descriptorPool,
                                   Span<const PipelineStage> supportedStages)
    : RHIResourceLayoutBase{ supportedStages }
    , ctx{ ctx }
    , descriptorPool{ descriptorPool }
{
    std::array<::vk::DescriptorType, MaxSamplerCount> descriptorTypes{};
    std::fill_n(descriptorTypes.begin(), staticSamplers.size(), ::vk::DescriptorType::eSampler);
    samplerSet = VkDescriptorSet(ctx, *descriptorPool->descriptorPool, descriptorTypes);
}

::vk::PipelineLayout VkResourceLayout::GetPipelineLayout()
{
    if (isDirty)
    {
        pipelineLayout = CreateLayout();
        isDirty = false;
    }
    return *pipelineLayout;
}

::vk::DescriptorSet VkResourceLayout::GetStaticSamplerDescriptorSet()
{
    if (isDirty)
    {
        pipelineLayout = CreateLayout();
        isDirty = false;
    }
    return *samplerSet->descriptorSet;
}

u32 VkResourceLayout::GetBytesPerStage()
{
    return maxLocalConstantsByteSize / supportedStages.size();
}

::vk::UniquePipelineLayout VkResourceLayout::CreateLayout()
{
    std::vector<::vk::PushConstantRange> pushRanges;

    if (supportedStages.size() == 1)
    {
        pushRanges.push_back(::vk::PushConstantRange{ .stageFlags = ::vk::ShaderStageFlagBits::eAll,
                                                      .offset = 0,
                                                      .size = maxLocalConstantsByteSize });
    }
    else
    {
        const u32 bytesPerStage = GetBytesPerStage();
        for (u32 i = 0; i < supportedStages.size(); i++)
        {
            pushRanges.push_back(
                ::vk::PushConstantRange{ .stageFlags = PipelineStageToShaderStageFlags(supportedStages[i]),
                                         .offset = i * bytesPerStage,
                                         .size = bytesPerStage });
        }
    }

    std::array layouts = { *descriptorPool->GetBindlessSet().descriptorLayout, *samplerSet->descriptorLayout };
    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = static_cast<u32>(layouts.size()),
                                               .pSetLayouts = layouts.data(),
                                               .pushConstantRangeCount = static_cast<u32>(pushRanges.size()),
                                               .pPushConstantRanges = pushRanges.data() };

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

    version++;

    return vkPipelineLayout;
}

} // namespace vex::vk