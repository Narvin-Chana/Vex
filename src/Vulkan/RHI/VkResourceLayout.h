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

    ::vk::UniquePipelineLayout pipelineLayout;
    const VkDescriptorSet& GetSamplerDescriptor();

private:
    ::vk::UniquePipelineLayout CreateLayout();
    MaybeUninitialized<VkDescriptorSet> samplerSet;
    std::vector<::vk::UniqueSampler> vkSamplers;

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<VkDescriptorPool> descriptorPool;
};

} // namespace vex::vk