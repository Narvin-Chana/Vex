#pragma once

#include <Vex/RHIImpl/RHIFeatureChecker.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct PhysicalDeviceInfo
{
    std::string deviceName;
    double dedicatedVideoMemoryMB;

    bool operator==(const PhysicalDeviceInfo& other) const
    {
        return other.deviceName == deviceName;
    }
};

struct RHIPhysicalDeviceBase
{
    // Criteria used to select the best device for the underlying RHI.
    bool operator>(const RHIPhysicalDeviceBase& other) const;

#if !VEX_SHIPPING
    void DumpPhysicalDeviceInfo();
#endif

    PhysicalDeviceInfo info;
    MaybeUninitialized<RHIFeatureChecker> featureChecker;
};

} // namespace vex