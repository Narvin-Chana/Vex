#include "VkFeatureChecker.h"

namespace vex::vk
{

VkFeatureChecker::~VkFeatureChecker() = default;

bool VkFeatureChecker::IsFeatureSupported(Feature feature)
{
    return false;
}

FeatureLevel VkFeatureChecker::GetFeatureLevel()
{
    return FeatureLevel();
}

ResourceBindingTier VkFeatureChecker::GetResourceBindingTier()
{
    return ResourceBindingTier();
}

} // namespace vex::vk