#pragma once

#include <string_view>

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

// IMPORTANT:
// Currently unsupported feature levels could be added later! I believe to start out its easier if we reduce the feature
// set we support (and focus on the most recent features.
// So no panic if the following seems overly restrictive.

enum class FeatureLevel : u8
{
    // Tiers lower than 12_1 are unsupported!
    Level_12_1,
    Level_12_2,
};

enum class ResourceBindingTier : u8
{
    // Tiers lower than 3 are unsupported!
    // ResourceTier1,
    // ResourceTier2,
    ResourceTier3,
};

enum class ShaderModel : u8
{
    Invalid,
    // SM_6_0,
    // SM_6_1,
    // SM_6_2,
    // SM_6_3,
    // SM_6_4,
    // SM_6_5,
    // Bindless ResourceDescriptorHeap is a must, this arrived in SM_6_6
    SM_6_6,
    // VK 1.3 dynamic rendering requires 6_7.
    SM_6_7,
    // TODO: add DX12 agility sdk to get support for the latest shading model (and features)
    // TODO: figure out how DXC's hlsl shading model extends to Vulkan, should be directly compatible but is important
    // to make sure.
    SM_6_8,
    SM_6_9,
};

class FeatureChecker
{
public:
    virtual ~FeatureChecker() = default;
    virtual std::string_view GetPhysicalDeviceName() = 0;
    virtual bool IsFeatureSupported(Feature feature) = 0;
    virtual FeatureLevel GetFeatureLevel() = 0;
    virtual ResourceBindingTier GetResourceBindingTier() = 0;
    virtual ShaderModel GetShaderModel() = 0;

#if !VEX_SHIPPING
    void DumpFeatureSupport();
#endif
};

} // namespace vex