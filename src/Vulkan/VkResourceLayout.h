#pragma once

#include "VkHeaders.h"

#include <Vex/RHI/RHIResourceLayout.h>

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