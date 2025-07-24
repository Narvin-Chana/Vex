#include "VkResourceLayout.h"

#include "Vex/ResourceBindingSet.h"
#include "VkDescriptorPool.h"
#include "VkErrorHandler.h"
#include "VkFeatureChecker.h"

#include <Vex/Debug.h>
#include <numeric>

namespace vex::vk
{
VkResourceLayout::VkResourceLayout(::vk::Device device,
                                   const VkDescriptorPool& descriptorPool,
                                   const VkFeatureChecker& featureChecker)
    : featureChecker{ featureChecker }
{
    ::vk::PushConstantRange range{ .stageFlags =
                                       ::vk::ShaderStageFlagBits::eAllGraphics | ::vk::ShaderStageFlagBits::eCompute,
                                   .offset = 0,
                                   .size = VkResourceLayout::GetMaxLocalConstantSize() };

    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = 1,
                                               .pSetLayouts = &*descriptorPool.bindlessLayout,
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    pipelineLayout = VEX_VK_CHECK <<= device.createPipelineLayoutUnique(createInfo);

    version++;
}

u32 VkResourceLayout::GetMaxLocalConstantSize() const
{
    const u32 maxBytes = featureChecker.GetMaxPushConstantSize();
    return std::max<u32>(0, maxBytes);
}
} // namespace vex::vk