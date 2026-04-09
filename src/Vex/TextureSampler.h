#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

static constexpr u32 GMaxBindlessSamplerCount = 2048;

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

struct TextureSamplerBase
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
    float minLOD = 0.0f;
    float maxLOD = std::numeric_limits<float>::max();

    constexpr bool operator==(const TextureSamplerBase&) const = default;
};

struct StaticTextureSampler : TextureSamplerBase
{
    BorderColor borderColor = BorderColor::TransparentBlackFloat;

    static constexpr StaticTextureSampler CreateSampler(FilterMode filterMode,
                                                        AddressMode addressMode,
                                                        float mipLODBias = 0.0f,
                                                        u32 maxAnisotropy = 1,
                                                        BorderColor borderColor = BorderColor::TransparentBlackFloat)
    {
        StaticTextureSampler sampler;
        sampler.minFilter = filterMode;
        sampler.magFilter = filterMode;
        sampler.mipFilter = filterMode;
        sampler.addressU = addressMode;
        sampler.addressV = addressMode;
        sampler.addressW = addressMode;
        sampler.mipLODBias = mipLODBias;
        sampler.maxAnisotropy = maxAnisotropy;
        sampler.borderColor = borderColor;
        return sampler;
    }
};

struct BindlessTextureSampler : TextureSamplerBase
{
    std::array<float, 4> borderColor;

    static constexpr BindlessTextureSampler CreateSampler(FilterMode filterMode,
                                                          AddressMode addressMode,
                                                          float mipLODBias = 0.0f,
                                                          u32 maxAnisotropy = 1,
                                                          const std::array<float, 4>& borderColor = {})
    {
        BindlessTextureSampler sampler;
        sampler.minFilter = filterMode;
        sampler.magFilter = filterMode;
        sampler.mipFilter = filterMode;
        sampler.addressU = addressMode;
        sampler.addressV = addressMode;
        sampler.addressW = addressMode;
        sampler.mipLODBias = mipLODBias;
        sampler.maxAnisotropy = maxAnisotropy;
        sampler.borderColor = borderColor;
        return sampler;
    }
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::TextureSamplerBase,
    VEX_HASH_COMBINE(seed, obj.minFilter);
    VEX_HASH_COMBINE(seed, obj.magFilter);
    VEX_HASH_COMBINE(seed, obj.mipFilter);
    VEX_HASH_COMBINE(seed, obj.addressU);
    VEX_HASH_COMBINE(seed, obj.addressV);
    VEX_HASH_COMBINE(seed, obj.addressW);
    VEX_HASH_COMBINE(seed, obj.mipLODBias);
    VEX_HASH_COMBINE(seed, obj.maxAnisotropy);
    VEX_HASH_COMBINE(seed, obj.compareOp);
    VEX_HASH_COMBINE(seed, obj.minLOD);
    VEX_HASH_COMBINE(seed, obj.maxLOD);
);

VEX_MAKE_HASHABLE(vex::StaticTextureSampler,
    VEX_HASH_COMBINE(seed, obj.borderColor);
);

VEX_MAKE_HASHABLE(vex::BindlessTextureSampler,
    VEX_HASH_COMBINE_CONTAINER(seed, obj.borderColor);
);

// clang-format on
