#include "PhysicalDevice.h"

#include <utility>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/Formattable.h>

namespace vex
{

#if !VEX_SHIPPING
void PhysicalDevice::DumpPhysicalDeviceInfo()
{
    VEX_LOG(
        Info,
        "Dumping feature checker support for physical device:\n\tDevice name: {}\n\tDedicated video memory (MB): {}\n"
        "\tMax feature level: {}\n"
        "\tResource binding tier: {}\n"
        "\tShader model: {}\n"
        "\tAdvanced Features:\n"
        "\t\tMesh Shaders: {}\n"
        "\t\tRayTracing: {}\n"
        "\t\tBindlessResources: {}",
        deviceName,
        dedicatedVideoMemoryMB,
        featureChecker->GetFeatureLevel(),
        featureChecker->GetResourceBindingTier(),
        featureChecker->GetShaderModel(),
        featureChecker->IsFeatureSupported(Feature::MeshShader),
        featureChecker->IsFeatureSupported(Feature::RayTracing),
        featureChecker->IsFeatureSupported(Feature::BindlessResources));
}
#endif

bool PhysicalDevice::operator>(const PhysicalDevice& other) const
{
    if (!featureChecker || !other.featureChecker)
    {
        VEX_LOG(Fatal, "Attempting to compare uninitialized physical devices.");
    }

    if (featureChecker->GetFeatureLevel() != other.featureChecker->GetFeatureLevel())
    {
        return std::to_underlying(featureChecker->GetFeatureLevel()) >
               std::to_underlying(other.featureChecker->GetFeatureLevel());
    }

    if (featureChecker->GetResourceBindingTier() != other.featureChecker->GetResourceBindingTier())
    {
        return std::to_underlying(featureChecker->GetResourceBindingTier()) >
               std::to_underlying(other.featureChecker->GetResourceBindingTier());
    }

    if (featureChecker->GetShaderModel() != other.featureChecker->GetShaderModel())
    {
        return std::to_underlying(featureChecker->GetShaderModel()) >
               std::to_underlying(other.featureChecker->GetShaderModel());
    }

    return dedicatedVideoMemoryMB > other.dedicatedVideoMemoryMB;
}

} // namespace vex
