#include "VkResourceLayout.h"

#include <Vex/ResourceBindingSet.h>
#include <Vex/Debug.h>
#include <Vex/PhysicalDevice.h>

#include <numeric>

#include "VkDescriptorPool.h"
#include "VkErrorHandler.h"
#include "VkFeatureChecker.h"

namespace vex::vk
{
VkResourceLayout::VkResourceLayout(::vk::Device device, const VkDescriptorPool& descriptorPool)
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

    // TODO(https://trello.com/c/SQBSUKw9): Add sampler support on the Vulkan side. This class contains the samplers,
    // now we just have to bind them.

    version++;
}

u32 VkResourceLayout::GetMaxLocalConstantSize() const
{
    const u32 maxBytes =
        reinterpret_cast<VkFeatureChecker*>(GPhysicalDevice->featureChecker.get())->GetMaxPushConstantSize();
    return std::max<u32>(0, maxBytes);
}
} // namespace vex::vk