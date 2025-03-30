#pragma once

#include <Vex/RHI.h>

#include <Vulkan/VkFeatureChecker.h>

namespace vex::vk
{

class VkRHI : public vex::RHI
{
public:
    virtual ~VkRHI() override;
    virtual FeatureChecker& GetFeatureChecker();

private:
    VkFeatureChecker featureChecker;
};

} // namespace vex::vk