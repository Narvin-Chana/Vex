#pragma once

#include <Vex/RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkFeatureChecker;
class VkDescriptorPool;

class VkResourceLayout : public RHIResourceLayout
{
public:
    VkResourceLayout(::vk::Device device, const VkDescriptorPool& descriptorPool);
    virtual u32 GetMaxLocalConstantSize() const override;

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk