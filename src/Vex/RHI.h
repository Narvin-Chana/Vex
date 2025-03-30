#pragma once

namespace vex
{

class FeatureChecker;

struct RenderHardwareInterface
{
    virtual ~RenderHardwareInterface() = default;
    virtual FeatureChecker& GetFeatureChecker() = 0;
};

using RHI = RenderHardwareInterface;

} // namespace vex