#pragma once

#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

enum class Feature : u8
{
    MeshShader,
    RayTracing,
    // TODO: add other features to this list.
};

// TODO: add more specific ray tracing support flags

// Lower feature levels are unsupported.
enum class FeatureLevel : u8
{
    Level_12_1,
    Level_12_2,
};

enum class ResourceBindingTier : u8
{
    ResourceTier1,
    ResourceTier2,
    ResourceTier3,
};

// Lower shading models are unsupported (bindless ResourceDescriptorHeap is a must).
enum class ShaderModel : u8
{
    SM_6_6,
    SM_6_7,
    // TODO: add DX12 agility sdk to get support for the latest shading model (and features)
    // TODO: figure out how DXC's hlsl shading model extends to Vulkan, should be directly compatible but is important
    // to make sure.
    SM_6_8,
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