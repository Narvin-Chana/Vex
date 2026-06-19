#include "RHIPhysicalDevice.h"

#include <utility>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/Utility/Formattable.h>

namespace vex
{

#if !VEX_SHIPPING
void RHIPhysicalDeviceBase::DumpPhysicalDeviceInfo()
{
    VEX_LOG(Info,
            "Dumping feature checker support for physical device:\n\tDevice name: {}\n\tVideo memory (MB): {}\n"
            "\tMax feature level: {}\n"
            "\tResource binding tier: {}\n"
            "\tShader model: {}\n"
            "\tAdvanced Features:\n"
            "\t\tMesh Shaders: {}\n"
            "\t\tRayTracing: {}\n",
            info.deviceName,
            info.videoMemoryMB,
            GetFeatureLevel(),
            GetResourceBindingTier(),
            GetShaderModel(),
            IsFeatureSupported(Feature::MeshShader),
            IsFeatureSupported(Feature::RayTracing));
}
#endif

bool RHIPhysicalDeviceBase::operator>(const RHIPhysicalDeviceBase& other) const
{
    if (GetFeatureLevel() != other.GetFeatureLevel())
    {
        return std::to_underlying(GetFeatureLevel()) > std::to_underlying(other.GetFeatureLevel());
    }

    if (GetResourceBindingTier() != other.GetResourceBindingTier())
    {
        return std::to_underlying(GetResourceBindingTier()) > std::to_underlying(other.GetResourceBindingTier());
    }

    if (GetShaderModel() != other.GetShaderModel())
    {
        return std::to_underlying(GetShaderModel()) > std::to_underlying(other.GetShaderModel());
    }

    const GPUDeviceType deviceType = GetDeviceType();
    const GPUDeviceType otherDeviceType = other.GetDeviceType();
    // Prioritize dedicated GPUs if we have feature parity.
    if (deviceType != otherDeviceType)
    {
        return std::to_underlying(deviceType) < std::to_underlying(otherDeviceType);
    }

    return info.videoMemoryMB > other.info.videoMemoryMB;
}

} // namespace vex
