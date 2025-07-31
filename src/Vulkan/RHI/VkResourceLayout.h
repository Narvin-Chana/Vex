#pragma once

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkFeatureChecker;
class VkDescriptorPool;

class VkResourceLayout final : public RHIResourceLayoutInterface
{
public:
    VkResourceLayout(::vk::Device device, const VkDescriptorPool& descriptorPool);
    virtual u32 GetMaxLocalConstantSize() const override;

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk