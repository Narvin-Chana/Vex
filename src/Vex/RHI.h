#pragma once

namespace vex
{

struct PlatformWindow;
class FeatureChecker;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual void InitWindow(const PlatformWindow& window) = 0;
    virtual FeatureChecker& GetFeatureChecker() = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex