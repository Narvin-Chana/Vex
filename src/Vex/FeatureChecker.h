#pragma once

#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

enum class Feature : u8
{
    Compute,
    MeshShader,
    RayTracing
};

enum class FeatureLevel : u8
{
    Level_11_0,
    Level_11_1,
    Level_12_0,
    Level_12_1,
    Level_12_2,
};

enum class ResourceBindingTier : u8
{
    ResourceTier0,
    ResourceTier1,
    ResourceTier2,
};

class FeatureChecker
{
public:
    virtual ~FeatureChecker() = default;
    virtual bool IsFeatureSupported(Feature feature) = 0;
    virtual FeatureLevel GetFeatureLevel() = 0;
    virtual ResourceBindingTier GetResourceBindingTier() = 0;
};

} // namespace vex