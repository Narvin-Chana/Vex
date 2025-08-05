#pragma once

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkFeatureChecker;
class VkDescriptorPool;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(::vk::Device device, const VkDescriptorPool& descriptorPool);

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk