#pragma once

#include <string>

#include <Vex/FeatureChecker.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct PhysicalDevice
{
    // Criteria used to select the best device for the underlying RHI.
    bool operator>(const PhysicalDevice& other) const;

#if !VEX_SHIPPING
    void DumpPhysicalDeviceInfo();
#endif

    std::string deviceName;
    double dedicatedVideoMemoryMB;
    UniqueHandle<FeatureChecker> featureChecker;
};

} // namespace vex