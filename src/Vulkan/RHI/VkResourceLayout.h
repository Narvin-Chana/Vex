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
    VkResourceLayout(NonNullPtr<VkGPUContext> ctx,
                     NonNullPtr<VkDescriptorPool> descriptorPool,
                     NonNullPtr<VkBindlessDescriptorSet> bindlessSet);

    ::vk::UniquePipelineLayout pipelineLayout;
    const RHIDescriptorSet& GetSamplerDescriptor();

private:
    ::vk::UniquePipelineLayout CreateLayout();
    std::optional<RHIDescriptorSet> samplerSet;
    std::vector<::vk::UniqueSampler> vkSamplers;

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<VkDescriptorPool> descriptorPool;
    NonNullPtr<VkBindlessDescriptorSet> bindlessSet;
};

} // namespace vex::vk