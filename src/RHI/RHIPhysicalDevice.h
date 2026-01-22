#pragma once

#include <string>

#include <Vex/RHIImpl/RHIFeatureChecker.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct RHIPhysicalDeviceBase
{
    // Criteria used to select the best device for the underlying RHI.
    bool operator>(const RHIPhysicalDeviceBase& other) const;

#if !VEX_SHIPPING
    void DumpPhysicalDeviceInfo();
#endif

    std::string deviceName;
    double dedicatedVideoMemoryMB;
    MaybeUninitialized<RHIFeatureChecker> featureChecker;
};

inline RHIPhysicalDevice* GPhysicalDevice = nullptr;

} // namespace vex