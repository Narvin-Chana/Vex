#pragma once

#include <Vex/NonNullPtr.h>

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkDescriptorPool;
struct VkGPUContext;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool);

    const VkDescriptorSet& GetSamplerDescriptorSet();
    ::vk::PipelineLayout GetPipelineLayout();

private:
    void RefreshCache();
    ::vk::UniquePipelineLayout CreateLayout();
    MaybeUninitialized<VkDescriptorSet> samplerSet;
    std::vector<::vk::UniqueSampler> vkSamplers;

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<VkDescriptorPool> samplerDescriptorPool;
    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk