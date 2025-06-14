#include "Formats.h"

#include <utility>

namespace vex
{

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
    std::unreachable();
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
    std::unreachable();
}

} // namespace vex