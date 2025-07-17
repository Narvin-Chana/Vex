#include "Formats.h"

#include <utility>

namespace vex
{

// Returns SRGB equivalent format. If not found returns unknown
TextureFormat GetSRGBEquivalentFormat(TextureFormat format)
{
    using enum TextureFormat;
    switch (format)
    {
    case RGBA8_UNORM:
        return RGBA8_UNORM_SRGB;
    case BGRA8_UNORM:
        return BGRA8_UNORM_SRGB;
    case BC1_UNORM:
        return BC1_UNORM_SRGB;
    case BC2_UNORM:
        return BC2_UNORM_SRGB;
    case BC3_UNORM:
        return BC3_UNORM_SRGB;
    case BC7_UNORM:
        return BC7_UNORM_SRGB;
    default:
        return UNKNOWN;
    }
}

bool IsFormatSRGB(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::BC1_UNORM_SRGB:
    case TextureFormat::BC2_UNORM_SRGB:
    case TextureFormat::BC3_UNORM_SRGB:
    case TextureFormat::BC7_UNORM_SRGB:
    case TextureFormat::BGRA8_UNORM_SRGB:
    case TextureFormat::RGBA8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool FormatHasSRGBEquivalent(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::BC1_UNORM:
    case TextureFormat::BC2_UNORM:
    case TextureFormat::BC3_UNORM:
    case TextureFormat::BC7_UNORM:
    case TextureFormat::BGRA8_UNORM:
    case TextureFormat::RGBA8_UNORM:
        return true;
    default:
        return false;
    }
}

bool FormatIsDepthStencilCompatible(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::D16_UNORM:
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D32_FLOAT_S8_UINT:
    case TextureFormat::D32_FLOAT:
        return true;
    default:
        return false;
    }
}

} // namespace vex