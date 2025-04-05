#pragma once

#include <dxgiformat.h>

#include <Vex/Formats.h>

namespace vex::dx12
{

// Convert from TextureFormat to DXGI_FORMAT
constexpr inline DXGI_FORMAT TextureFormatToDXGI(TextureFormat format)
{
    switch (format)
    {
    // Standard formats
    case TextureFormat::R8_UNORM:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::R8_SNORM:
        return DXGI_FORMAT_R8_SNORM;
    case TextureFormat::R8_UINT:
        return DXGI_FORMAT_R8_UINT;
    case TextureFormat::R8_SINT:
        return DXGI_FORMAT_R8_SINT;
    case TextureFormat::RG8_UNORM:
        return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RG8_SNORM:
        return DXGI_FORMAT_R8G8_SNORM;
    case TextureFormat::RG8_UINT:
        return DXGI_FORMAT_R8G8_UINT;
    case TextureFormat::RG8_SINT:
        return DXGI_FORMAT_R8G8_SINT;
    case TextureFormat::RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::RGBA8_SNORM:
        return DXGI_FORMAT_R8G8B8A8_SNORM;
    case TextureFormat::RGBA8_UINT:
        return DXGI_FORMAT_R8G8B8A8_UINT;
    case TextureFormat::RGBA8_SINT:
        return DXGI_FORMAT_R8G8B8A8_SINT;
    case TextureFormat::BGRA8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    // 16-bit formats
    case TextureFormat::R16_UINT:
        return DXGI_FORMAT_R16_UINT;
    case TextureFormat::R16_SINT:
        return DXGI_FORMAT_R16_SINT;
    case TextureFormat::R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::RG16_UINT:
        return DXGI_FORMAT_R16G16_UINT;
    case TextureFormat::RG16_SINT:
        return DXGI_FORMAT_R16G16_SINT;
    case TextureFormat::RG16_FLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA16_UINT:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16_SINT:
        return DXGI_FORMAT_R16G16B16A16_SINT;
    case TextureFormat::RGBA16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;

    // 32-bit formats
    case TextureFormat::R32_UINT:
        return DXGI_FORMAT_R32_UINT;
    case TextureFormat::R32_SINT:
        return DXGI_FORMAT_R32_SINT;
    case TextureFormat::R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::RG32_UINT:
        return DXGI_FORMAT_R32G32_UINT;
    case TextureFormat::RG32_SINT:
        return DXGI_FORMAT_R32G32_SINT;
    case TextureFormat::RG32_FLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGB32_UINT:
        return DXGI_FORMAT_R32G32B32_UINT;
    case TextureFormat::RGB32_SINT:
        return DXGI_FORMAT_R32G32B32_SINT;
    case TextureFormat::RGB32_FLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::RGBA32_UINT:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32_SINT:
        return DXGI_FORMAT_R32G32B32A32_SINT;
    case TextureFormat::RGBA32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

    // Packed formats
    case TextureFormat::RGB10A2_UNORM:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TextureFormat::RGB10A2_UINT:
        return DXGI_FORMAT_R10G10B10A2_UINT;
    case TextureFormat::RG11B10_FLOAT:
        return DXGI_FORMAT_R11G11B10_FLOAT;

    // Depth/stencil formats
    case TextureFormat::D16_UNORM:
        return DXGI_FORMAT_D16_UNORM;
    case TextureFormat::D24_UNORM_S8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::D32_FLOAT_S8_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    // BC compressed formats
    case TextureFormat::BC1_UNORM:
        return DXGI_FORMAT_BC1_UNORM;
    case TextureFormat::BC1_UNORM_SRGB:
        return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TextureFormat::BC2_UNORM:
        return DXGI_FORMAT_BC2_UNORM;
    case TextureFormat::BC2_UNORM_SRGB:
        return DXGI_FORMAT_BC2_UNORM_SRGB;
    case TextureFormat::BC3_UNORM:
        return DXGI_FORMAT_BC3_UNORM;
    case TextureFormat::BC3_UNORM_SRGB:
        return DXGI_FORMAT_BC3_UNORM_SRGB;
    case TextureFormat::BC4_UNORM:
        return DXGI_FORMAT_BC4_UNORM;
    case TextureFormat::BC4_SNORM:
        return DXGI_FORMAT_BC4_SNORM;
    case TextureFormat::BC5_UNORM:
        return DXGI_FORMAT_BC5_UNORM;
    case TextureFormat::BC5_SNORM:
        return DXGI_FORMAT_BC5_SNORM;
    case TextureFormat::BC6H_UF16:
        return DXGI_FORMAT_BC6H_UF16;
    case TextureFormat::BC6H_SF16:
        return DXGI_FORMAT_BC6H_SF16;
    case TextureFormat::BC7_UNORM:
        return DXGI_FORMAT_BC7_UNORM;
    case TextureFormat::BC7_UNORM_SRGB:
        return DXGI_FORMAT_BC7_UNORM_SRGB;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

// Convert from DXGI_FORMAT to TextureFormat
constexpr inline TextureFormat DXGIToTextureFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    // Standard formats
    case DXGI_FORMAT_R8_UNORM:
        return TextureFormat::R8_UNORM;
    case DXGI_FORMAT_R8_SNORM:
        return TextureFormat::R8_SNORM;
    case DXGI_FORMAT_R8_UINT:
        return TextureFormat::R8_UINT;
    case DXGI_FORMAT_R8_SINT:
        return TextureFormat::R8_SINT;
    case DXGI_FORMAT_R8G8_UNORM:
        return TextureFormat::RG8_UNORM;
    case DXGI_FORMAT_R8G8_SNORM:
        return TextureFormat::RG8_SNORM;
    case DXGI_FORMAT_R8G8_UINT:
        return TextureFormat::RG8_UINT;
    case DXGI_FORMAT_R8G8_SINT:
        return TextureFormat::RG8_SINT;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return TextureFormat::RGBA8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return TextureFormat::RGBA8_UNORM_SRGB;
    case DXGI_FORMAT_R8G8B8A8_SNORM:
        return TextureFormat::RGBA8_SNORM;
    case DXGI_FORMAT_R8G8B8A8_UINT:
        return TextureFormat::RGBA8_UINT;
    case DXGI_FORMAT_R8G8B8A8_SINT:
        return TextureFormat::RGBA8_SINT;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return TextureFormat::BGRA8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return TextureFormat::BGRA8_UNORM_SRGB;

    // 16-bit formats
    case DXGI_FORMAT_R16_UINT:
        return TextureFormat::R16_UINT;
    case DXGI_FORMAT_R16_SINT:
        return TextureFormat::R16_SINT;
    case DXGI_FORMAT_R16_FLOAT:
        return TextureFormat::R16_FLOAT;
    case DXGI_FORMAT_R16G16_UINT:
        return TextureFormat::RG16_UINT;
    case DXGI_FORMAT_R16G16_SINT:
        return TextureFormat::RG16_SINT;
    case DXGI_FORMAT_R16G16_FLOAT:
        return TextureFormat::RG16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_UINT:
        return TextureFormat::RGBA16_UINT;
    case DXGI_FORMAT_R16G16B16A16_SINT:
        return TextureFormat::RGBA16_SINT;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return TextureFormat::RGBA16_FLOAT;

    // 32-bit formats
    case DXGI_FORMAT_R32_UINT:
        return TextureFormat::R32_UINT;
    case DXGI_FORMAT_R32_SINT:
        return TextureFormat::R32_SINT;
    case DXGI_FORMAT_R32_FLOAT:
        return TextureFormat::R32_FLOAT;
    case DXGI_FORMAT_R32G32_UINT:
        return TextureFormat::RG32_UINT;
    case DXGI_FORMAT_R32G32_SINT:
        return TextureFormat::RG32_SINT;
    case DXGI_FORMAT_R32G32_FLOAT:
        return TextureFormat::RG32_FLOAT;
    case DXGI_FORMAT_R32G32B32_UINT:
        return TextureFormat::RGB32_UINT;
    case DXGI_FORMAT_R32G32B32_SINT:
        return TextureFormat::RGB32_SINT;
    case DXGI_FORMAT_R32G32B32_FLOAT:
        return TextureFormat::RGB32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_UINT:
        return TextureFormat::RGBA32_UINT;
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return TextureFormat::RGBA32_SINT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return TextureFormat::RGBA32_FLOAT;

    // Packed formats
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return TextureFormat::RGB10A2_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UINT:
        return TextureFormat::RGB10A2_UINT;
    case DXGI_FORMAT_R11G11B10_FLOAT:
        return TextureFormat::RG11B10_FLOAT;

    // Depth/stencil formats
    case DXGI_FORMAT_D16_UNORM:
        return TextureFormat::D16_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return TextureFormat::D24_UNORM_S8_UINT;
    case DXGI_FORMAT_D32_FLOAT:
        return TextureFormat::D32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return TextureFormat::D32_FLOAT_S8_UINT;

    // BC compressed formats
    case DXGI_FORMAT_BC1_UNORM:
        return TextureFormat::BC1_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return TextureFormat::BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:
        return TextureFormat::BC2_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB:
        return TextureFormat::BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:
        return TextureFormat::BC3_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        return TextureFormat::BC3_UNORM_SRGB;
    case DXGI_FORMAT_BC4_UNORM:
        return TextureFormat::BC4_UNORM;
    case DXGI_FORMAT_BC4_SNORM:
        return TextureFormat::BC4_SNORM;
    case DXGI_FORMAT_BC5_UNORM:
        return TextureFormat::BC5_UNORM;
    case DXGI_FORMAT_BC5_SNORM:
        return TextureFormat::BC5_SNORM;
    case DXGI_FORMAT_BC6H_UF16:
        return TextureFormat::BC6H_UF16;
    case DXGI_FORMAT_BC6H_SF16:
        return TextureFormat::BC6H_SF16;
    case DXGI_FORMAT_BC7_UNORM:
        return TextureFormat::BC7_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return TextureFormat::BC7_UNORM_SRGB;

    default:
        return TextureFormat::UNKNOWN;
    }
}

} // namespace vex::dx12
