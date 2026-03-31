#include "VkResourceLayout.h"

#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Debug.h>
#include <Vex/RHIImpl/RHIBuffer.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

VkResourceLayout::VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool)
    : ctx{ ctx }
    , descriptorPool{ descriptorPool }
    , pipelineLayout{ CreateLayout() }
{
}

const ::vk::UniquePipelineLayout& VkResourceLayout::GetPipelineLayout() const
{
    return pipelineLayout;
}

::vk::UniquePipelineLayout VkResourceLayout::CreateLayout()
{
    ::vk::PushConstantRange range{ .stageFlags =
                                       ::vk::ShaderStageFlagBits::eAllGraphics | ::vk::ShaderStageFlagBits::eCompute,
                                   .offset = 0,
                                   .size = GPhysicalDevice->GetMaxLocalConstantsByteSize() };

    std::array layouts = { *descriptorPool->GetBindlessSet().descriptorLayout };
    ::vk::PipelineLayoutCreateInfo createInfo{ .setLayoutCount = static_cast<u32>(layouts.size()),
                                               .pSetLayouts = layouts.data(),
                                               .pushConstantRangeCount = 1,
                                               .pPushConstantRanges = &range };

    ::vk::UniquePipelineLayout vkPipelineLayout = VEX_VK_CHECK <<= ctx->device.createPipelineLayoutUnique(createInfo);

    version++;
    isDirty = false;

    return vkPipelineLayout;
}

} // namespace vex::vk