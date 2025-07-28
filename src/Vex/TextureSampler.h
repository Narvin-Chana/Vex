#pragma once

#include <string>

#include <Vex/GraphicsPipeline.h>
#include <Vex/Types.h>

namespace vex
{

enum class FilterMode : u8
{
    Point,
    Linear,
    Anisotropic
};

enum class AddressMode : u8
{
    Wrap,
    Mirror,
    Clamp,
    Border,
    MirrorOnce
};

enum class BorderColor : u8
{
    TransparentBlackFloat,
    TransparentBlackInt,
    OpaqueBlackFloat,
    OpaqueBlackInt,
    OpaqueWhiteFloat,
    OpaqueWhiteInt,
};

struct TextureSampler
{
    // Name of the sampler, the one that is used inside your shader.
    std::string name;
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    FilterMode mipFilter = FilterMode::Linear;
    AddressMode addressU = AddressMode::Wrap;
    AddressMode addressV = AddressMode::Wrap;
    AddressMode addressW = AddressMode::Wrap;
    float mipLODBias = 0.0f;
    u32 maxAnisotropy = 1;
    CompareOp compareOp = CompareOp::Never;
    BorderColor borderColor = BorderColor::TransparentBlackFloat;
    float minLOD = 0.0f;
    float maxLOD = std::numeric_limits<float>::max();
};

} // namespace vex