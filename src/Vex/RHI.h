#pragma once

namespace vex
{

struct PlatformWindow;
class FeatureChecker;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual FeatureChecker& GetFeatureChecker() = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex