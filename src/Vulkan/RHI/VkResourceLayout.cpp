#include "VkResourceLayout.h"

#include <utility>

#include <Vex/Debug.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/ResourceBindingSet.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx, const VkDescriptorPool& descriptorPool)
{
    ::vk::PushConstantRange range{ .stageFlags =
                                       ::vk::ShaderStageFlagBits::eAllGraphics | ::vk::ShaderStageFlagBits::eCompute,
                                   .offset = 0,
                                   .size = GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize() };

    std::array<::vk::DescriptorSetLayout, 2> setLayouts = { *descriptorPool.bindlessLayout,
                                                            *descriptorPool.bindlessMappingLayout };

    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = setLayouts.size(),
                                               .pSetLayouts = setLayouts.data(),
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    pipelineLayout = VEX_VK_CHECK <<= ctx->device.createPipelineLayoutUnique(createInfo);

    // TODO(https://trello.com/c/SQBSUKw9): Add sampler support on the Vulkan side. This class contains the samplers,
    // now we just have to bind them.

    version++;
}

} // namespace vex::vk