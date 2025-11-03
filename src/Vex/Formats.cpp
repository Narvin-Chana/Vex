#include "Formats.h"

#include <utility>

#include <Vex/Logger.h>

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

bool DoesFormatSupportStencil(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D32_FLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

std::string_view GetFormatHLSLType(TextureFormat format)
{
    switch (format)
    {
    // 8-bit single channel
    case TextureFormat::R8_UNORM:
    case TextureFormat::R8_SNORM:
        return "float";
    case TextureFormat::R8_UINT:
        return "uint";
    case TextureFormat::R8_SINT:
        return "int";

    // 8-bit dual channel
    case TextureFormat::RG8_UNORM:
    case TextureFormat::RG8_SNORM:
        return "float2";
    case TextureFormat::RG8_UINT:
        return "uint2";
    case TextureFormat::RG8_SINT:
        return "int2";

    // 8-bit quad channel
    case TextureFormat::RGBA8_UNORM:
    case TextureFormat::RGBA8_UNORM_SRGB:
    case TextureFormat::RGBA8_SNORM:
    case TextureFormat::BGRA8_UNORM:
    case TextureFormat::BGRA8_UNORM_SRGB:
        return "float4";
    case TextureFormat::RGBA8_UINT:
        return "uint4";
    case TextureFormat::RGBA8_SINT:
        return "int4";

    // 16-bit single channel
    case TextureFormat::R16_UINT:
        return "uint";
    case TextureFormat::R16_SINT:
        return "int";
    case TextureFormat::R16_FLOAT:
        return "min16float";

    // 16-bit dual channel
    case TextureFormat::RG16_UINT:
        return "uint2";
    case TextureFormat::RG16_SINT:
        return "int2";
    case TextureFormat::RG16_FLOAT:
        return "min16float2";

    // 16-bit quad channel
    case TextureFormat::RGBA16_UINT:
        return "uint4";
    case TextureFormat::RGBA16_SINT:
        return "int4";
    case TextureFormat::RGBA16_FLOAT:
        return "min16float4";

    // 32-bit single channel
    case TextureFormat::R32_UINT:
        return "uint";
    case TextureFormat::R32_SINT:
        return "int";
    case TextureFormat::R32_FLOAT:
        return "float";

    // 32-bit dual channel
    case TextureFormat::RG32_UINT:
        return "uint2";
    case TextureFormat::RG32_SINT:
        return "int2";
    case TextureFormat::RG32_FLOAT:
        return "float2";

    // 32-bit triple channel
    case TextureFormat::RGB32_UINT:
        return "uint3";
    case TextureFormat::RGB32_SINT:
        return "int3";
    case TextureFormat::RGB32_FLOAT:
        return "float3";

    // 32-bit quad channel
    case TextureFormat::RGBA32_UINT:
        return "uint4";
    case TextureFormat::RGBA32_SINT:
        return "int4";
    case TextureFormat::RGBA32_FLOAT:
        return "float4";

    // Packed formats
    case TextureFormat::RGB10A2_UNORM:
        return "float4";
    case TextureFormat::RGB10A2_UINT:
        return "uint4";
    case TextureFormat::RG11B10_FLOAT:
        return "min16float3";

    // Depth/stencil formats (typically sampled as float)
    case TextureFormat::D16_UNORM:
    case TextureFormat::D32_FLOAT:
        return "float";
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D32_FLOAT_S8_UINT:
        return "float2"; // depth + stencil

    // BC compressed formats (all decompress to float4)
    case TextureFormat::BC1_UNORM:
    case TextureFormat::BC1_UNORM_SRGB:
    case TextureFormat::BC2_UNORM:
    case TextureFormat::BC2_UNORM_SRGB:
    case TextureFormat::BC3_UNORM:
    case TextureFormat::BC3_UNORM_SRGB:
    case TextureFormat::BC7_UNORM:
    case TextureFormat::BC7_UNORM_SRGB:
        return "float4";

    // BC4 is single channel
    case TextureFormat::BC4_UNORM:
    case TextureFormat::BC4_SNORM:
        return "float";

    // BC5 is dual channel
    case TextureFormat::BC5_UNORM:
    case TextureFormat::BC5_SNORM:
        return "float2";

    // BC6H is HDR RGB
    case TextureFormat::BC6H_UF16:
    case TextureFormat::BC6H_SF16:
        return "float3";

    // Error case
    case TextureFormat::UNKNOWN:
    default:
        VEX_LOG(Fatal, "Invalid or unsupported format!");
        return "";
    }
    std::unreachable();
}

bool IsFormatBlockCompressed(TextureFormat format)
{
    return format >= TextureFormat::BC1_UNORM && format <= TextureFormat::BC7_UNORM_SRGB;
}

bool DoesFormatSupportMipGeneration(TextureFormat format)
{
    using enum TextureFormat;

    VEX_ASSERT(format != UNKNOWN, "Unknown is an invalid texture format!");

    // Depth-stencil textures are unsupported for mip generation.
    if (FormatIsDepthStencilCompatible(format))
    {
        return false;
    }

    // Block-compressed formats cannot be directly written to.
    if (IsFormatBlockCompressed(format))
    {
        return false;
    }

    switch (format)
    {
    // All UINT/SINT formats are unable to be correctly linearly filtered.
    case R8_UINT:
    case R8_SINT:
    case RG8_UINT:
    case RG8_SINT:
    case RGBA8_UINT:
    case RGBA8_SINT:
    case R16_UINT:
    case R16_SINT:
    case RG16_UINT:
    case RG16_SINT:
    case RGBA16_UINT:
    case RGBA16_SINT:
    case R32_UINT:
    case R32_SINT:
    case RG32_UINT:
    case RG32_SINT:
    case RGB32_UINT:
    case RGB32_SINT:
    case RGBA32_UINT:
    case RGBA32_SINT:
    case RGB10A2_UINT:
        return false;
    default:
        break;
    }

    return true;
}

} // namespace vex