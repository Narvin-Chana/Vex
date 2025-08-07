#pragma once

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkFeatureChecker;
class VkDescriptorPool;
struct VkGPUContext;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(VkGPUContext& ctx, const VkDescriptorPool& descriptorPool);

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk