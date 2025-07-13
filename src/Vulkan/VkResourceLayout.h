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
    virtual void Update(const ResourceBindingSet& set) override;

    ::vk::UniquePipelineLayout pipelineLayout;

    u32 reservedLocalConstantSize = 0;
};

} // namespace vex::vk