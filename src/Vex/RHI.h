#pragma once

#include <vector>

#include <Vex/PhysicalDevice.h>

namespace vex
{

struct PlatformWindow;
class FeatureChecker;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex