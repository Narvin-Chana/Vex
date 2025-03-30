#include "VkRHI.h"

namespace vex::vk
{

VkRHI::~VkRHI() = default;

FeatureChecker& VkRHI::GetFeatureChecker()
{
    return featureChecker;
}

} // namespace vex::vk