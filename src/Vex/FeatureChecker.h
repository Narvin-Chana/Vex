#pragma once

#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

enum class Feature : u8
{
    MeshShader,
    RayTracing,
    BindlessResources,
    MipGeneration,
    DepthStencilReadback
    // Additional features go here...
};

// TODO(https://trello.com/c/KnVRsH9P): add more specific ray tracing support flags

// IMPORTANT:
// Currently unsupported feature levels could be added later! I believe to start out its easier if we reduce the feature
// set we support (and focus on the most recent features.
// So no panic if the following seems overly restrictive.

enum class FeatureLevel : u8
{
    // Tiers lower than 12_0 are unsupported!
    Level_12_0,
    Level_12_1,
    Level_12_2,
};

enum class ResourceBindingTier : u8
{
    ResourceTier1,
    ResourceTier2,
    ResourceTier3,
};

enum class ShaderModel : u8
{
    SM_6_0,
    SM_6_1,
    SM_6_2,
    SM_6_3,
    SM_6_4,
    SM_6_5,
    // For bindless the ResourceDescriptorHeap is a must, this arrived in SM_6_6.
    SM_6_6,
    // VK 1.3 dynamic rendering requires SM_6_7.
    SM_6_7,
    SM_6_8,
    SM_6_9,
};

enum class TextureFormat : u8;
class FeatureChecker
{
public:
    virtual ~FeatureChecker() = default;
    virtual bool IsFeatureSupported(Feature feature) const = 0;
    virtual FeatureLevel GetFeatureLevel() const = 0;
    virtual ResourceBindingTier GetResourceBindingTier() const = 0;
    virtual ShaderModel GetShaderModel() const = 0;
    virtual u32 GetMaxLocalConstantsByteSize() const = 0;
    virtual bool FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const = 0;
};

} // namespace vex