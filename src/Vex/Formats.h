#pragma once

#include <Vex/Types.h>

namespace vex
{

enum class TextureFormat : u8
{
    // Standard formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RGBA8_UNORM,
    RGBA8_UNORM_SRGB,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    BGRA8_UNORM,
    BGRA8_UNORM_SRGB,

    // 16-bit formats
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    RG16_UINT,
    RG16_SINT,
    RG16_FLOAT,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,

    // 32-bit formats
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    // Packed formats
    RGB10A2_UNORM,
    RGB10A2_UINT,
    RG11B10_FLOAT,

    // Depth/stencil formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,

    // BC compressed formats
    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UF16,
    BC6H_SF16,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    // Error format
    UNKNOWN,
};

} // namespace vex
