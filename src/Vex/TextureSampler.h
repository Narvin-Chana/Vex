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
    FilterMode minFilter = FilterMode::Point;
    FilterMode magFilter = FilterMode::Point;
    FilterMode mipFilter = FilterMode::Point;
    AddressMode addressU = AddressMode::Wrap;
    AddressMode addressV = AddressMode::Wrap;
    AddressMode addressW = AddressMode::Wrap;
    float mipLODBias = 0.0f;
    u32 maxAnisotropy = 1;
    CompareOp compareOp = CompareOp::Never;
    BorderColor borderColor = BorderColor::TransparentBlackFloat;
    float minLOD = 0.0f;
    float maxLOD = std::numeric_limits<float>::max();

    static constexpr TextureSampler CreateSampler(FilterMode filterMode,
                                                  AddressMode addressMode,
                                                  float mipLODBias = 0.0f,
                                                  u32 maxAnisotropy = 1)
    {
        return {
            .minFilter = filterMode,
            .magFilter = filterMode,
            .mipFilter = filterMode,
            .addressU = addressMode,
            .addressV = addressMode,
            .addressW = addressMode,
            .mipLODBias = mipLODBias,
            .maxAnisotropy = maxAnisotropy,
        };
    }
};

} // namespace vex