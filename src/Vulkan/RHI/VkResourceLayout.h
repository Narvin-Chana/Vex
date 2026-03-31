#pragma once

#include <Vex/Utility/MaybeUninitialized.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/RHI/VkDescriptorSet.h>
#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkDescriptorPool;
struct VkGPUContext;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool);

    ::vk::PipelineLayout GetPipelineLayout();
    ::vk::DescriptorSet GetStaticSamplerDescriptorSet();

    static ::vk::ShaderStageFlags GetPushConstantStageFlags();

private:
    ::vk::UniquePipelineLayout CreateLayout();
    MaybeUninitialized<VkDescriptorSet> samplerSet;
    std::vector<::vk::UniqueSampler> vkSamplers;

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<VkDescriptorPool> descriptorPool;
    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk