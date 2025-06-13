#include "VkResourceLayout.h"

#include "VkDescriptorPool.h"
#include "VkErrorHandler.h"
#include "VkFeatureChecker.h"

#include <Vex/Debug.h>

namespace vex::vk
{
VkResourceLayout::VkResourceLayout(::vk::Device device,
                                   const VkDescriptorPool& descriptorPool,
                                   const VkFeatureChecker& featureChecker)
    : featureChecker{ featureChecker }
{
    ::vk::PushConstantRange range{ .stageFlags = ::vk::ShaderStageFlagBits::eAllGraphics,
                                   .offset = 0,
                                   .size = VkResourceLayout::GetMaxLocalConstantSize() };

    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = 1,
                                               .pSetLayouts = &*descriptorPool.bindlessLayout,
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    pipelineLayout = VEX_VK_CHECK <<= device.createPipelineLayoutUnique(createInfo);

    version++;
}

bool VkResourceLayout::ValidateGlobalConstant(const GlobalConstant& globalConstant) const
{
    if (!RHIResourceLayout::ValidateGlobalConstant(globalConstant))
    {
        return false;
    }

    return true;
}
u32 VkResourceLayout::GetMaxLocalConstantSize() const
{
    const u32 maxBytes = featureChecker.GetMaxPushConstantSize();

    // TODO: Consider global constant in the available size
    return std::max<u32>(0, maxBytes);
}
} // namespace vex::vk