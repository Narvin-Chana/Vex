#pragma once

#include <Vex/FeatureChecker.h>

namespace vex::vk
{
class VkFeatureChecker : public vex::FeatureChecker
{
public:
    virtual ~VkFeatureChecker();
    virtual bool IsFeatureSupported(Feature feature) override;
    virtual FeatureLevel GetFeatureLevel() override;
    virtual ResourceBindingTier GetResourceBindingTier() override;
};
} // namespace vex::vk