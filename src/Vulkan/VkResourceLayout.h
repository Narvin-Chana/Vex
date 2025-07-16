#pragma once

#include "VkHeaders.h"

#include <Vex/RHI/RHIResourceLayout.h>

namespace vex::vk
{
class VkFeatureChecker;
class VkDescriptorPool;

class VkResourceLayout : public RHIResourceLayout
{
    const VkFeatureChecker& featureChecker;

public:
    VkResourceLayout(::vk::Device device,
                     const VkDescriptorPool& descriptorPool,
                     const VkFeatureChecker& featureChecker);

    virtual u32 GetLocalConstantsOffset() const noexcept override;
    virtual u32 GetMaxLocalConstantSize() const override;

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk