#pragma once

#include <utility>

#include <Vex/Formats.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

// Certain formats require special care because they differ from DX12 to Vulkan in terms of component ordering.
// This method returns true if the format has ordering different to what is found in vex::TextureFormat.
constexpr inline bool IsSpecialFormat(::vk::Format format)
{
    using enum ::vk::Format;
    // Packed formats
    switch (format)
    {
    case eA2B10G10R10UnormPack32:
    case eA2B10G10R10UintPack32:
    case eB10G11R11UfloatPack32:
        return true;
    default:
        return false;
    }
}

// Convert from TextureFormat to VkFormat
constexpr ::vk::Format TextureFormatToVulkan(TextureFormat format, bool isSRGB)
{
    using enum ::vk::Format;
    using enum TextureFormat;
    switch (format)
    {
    // Standard formats
    case R8_UNORM:
        return eR8Unorm;
    case R8_SNORM:
        return eR8Snorm;
    case R8_UINT:
        return eR8Uint;
    case R8_SINT:
        return eR8Sint;
    case RG8_UNORM:
        return eR8G8Unorm;
    case RG8_SNORM:
        return eR8G8Snorm;
    case RG8_UINT:
        return eR8G8Uint;
    case RG8_SINT:
        return eR8G8Sint;
    case RGBA8_UNORM:
        return !isSRGB ? eR8G8B8A8Unorm : eR8G8B8A8Srgb;
    case RGBA8_SNORM:
        return eR8G8B8A8Snorm;
    case RGBA8_UINT:
        return eR8G8B8A8Uint;
    case RGBA8_SINT:
        return eR8G8B8A8Sint;
    case BGRA8_UNORM:
        return !isSRGB ? eB8G8R8A8Unorm : eB8G8R8A8Srgb;

    // 16-bit formats
    case R16_UINT:
        return eR16Uint;
    case R16_SINT:
        return eR16Sint;
    case R16_FLOAT:
        return eR16Sfloat;
    case RG16_UINT:
        return eR16G16Uint;
    case RG16_SINT:
        return eR16G16Sint;
    case RG16_FLOAT:
        return eR16G16Sfloat;
    case RGBA16_UINT:
        return eR16G16B16A16Uint;
    case RGBA16_SINT:
        return eR16G16B16A16Sint;
    case RGBA16_FLOAT:
        return eR16G16B16A16Sfloat;

    // 32-bit formats
    case R32_UINT:
        return eR32Uint;
    case R32_SINT:
        return eR32Sint;
    case R32_FLOAT:
        return eR32Sfloat;
    case RG32_UINT:
        return eR32G32Uint;
    case RG32_SINT:
        return eR32G32Sint;
    case RG32_FLOAT:
        return eR32G32Sfloat;
    case RGB32_UINT:
        return eR32G32B32Uint;
    case RGB32_SINT:
        return eR32G32B32Sint;
    case RGB32_FLOAT:
        return eR32G32B32Sfloat;
    case RGBA32_UINT:
        return eR32G32B32A32Uint;
    case RGBA32_SINT:
        return eR32G32B32A32Sint;
    case RGBA32_FLOAT:
        return eR32G32B32A32Sfloat;

    // Packed formats
    case RGB10A2_UNORM:
        return eA2B10G10R10UnormPack32; // Note format difference
    case RGB10A2_UINT:
        return eA2B10G10R10UintPack32; // Note format difference
    case RG11B10_FLOAT:
        return eB10G11R11UfloatPack32; // Note format difference

    // Depth/stencil formats
    case D16_UNORM:
        return eD16Unorm;
    case D24_UNORM_S8_UINT:
        return eD24UnormS8Uint;
    case D32_FLOAT:
        return eD32Sfloat;
    case D32_FLOAT_S8_UINT:
        return eD32SfloatS8Uint;

    // BC compressed formats
    case BC1_UNORM:
        return !isSRGB ? eBc1RgbaUnormBlock : eBc1RgbaSrgbBlock;
    case BC2_UNORM:
        return !isSRGB ? eBc2UnormBlock : eBc2SrgbBlock;
    case BC3_UNORM:
        return !isSRGB ? eBc3UnormBlock : eBc3SrgbBlock;
    case BC4_UNORM:
        return eBc4UnormBlock;
    case BC4_SNORM:
        return eBc4SnormBlock;
    case BC5_UNORM:
        return eBc5UnormBlock;
    case BC5_SNORM:
        return eBc5SnormBlock;
    case BC6H_UF16:
        return eBc6HUfloatBlock;
    case BC6H_SF16:
        return eBc6HSfloatBlock;
    case BC7_UNORM:
        return !isSRGB ? eBc7UnormBlock : eBc7SrgbBlock;

    default:
        return eUndefined;
    }
}

// Convert from VkFormat to TextureFormat
constexpr inline TextureFormat VulkanToTextureFormat(::vk::Format format)
{
    using enum TextureFormat;
    using enum ::vk::Format;
    switch (format)
    {
    // Standard formats
    case eR8Unorm:
        return R8_UNORM;
    case eR8Snorm:
        return R8_SNORM;
    case eR8Uint:
        return R8_UINT;
    case eR8Sint:
        return R8_SINT;
    case eR8G8Unorm:
        return RG8_UNORM;
    case eR8G8Snorm:
        return RG8_SNORM;
    case eR8G8Uint:
        return RG8_UINT;
    case eR8G8Sint:
        return RG8_SINT;
    case eR8G8B8A8Unorm:
    case eR8G8B8A8Srgb:
        return RGBA8_UNORM;
    case eR8G8B8A8Snorm:
        return RGBA8_SNORM;
    case eR8G8B8A8Uint:
        return RGBA8_UINT;
    case eR8G8B8A8Sint:
        return RGBA8_SINT;
    case eB8G8R8A8Unorm:
    case eB8G8R8A8Srgb:
        return BGRA8_UNORM;

    // 16-bit formats
    case eR16Uint:
        return R16_UINT;
    case eR16Sint:
        return R16_SINT;
    case eR16Sfloat:
        return R16_FLOAT;
    case eR16G16Uint:
        return RG16_UINT;
    case eR16G16Sint:
        return RG16_SINT;
    case eR16G16Sfloat:
        return RG16_FLOAT;
    case eR16G16B16A16Uint:
        return RGBA16_UINT;
    case eR16G16B16A16Sint:
        return RGBA16_SINT;
    case eR16G16B16A16Sfloat:
        return RGBA16_FLOAT;

    // 32-bit formats
    case eR32Uint:
        return R32_UINT;
    case eR32Sint:
        return R32_SINT;
    case eR32Sfloat:
        return R32_FLOAT;
    case eR32G32Uint:
        return RG32_UINT;
    case eR32G32Sint:
        return RG32_SINT;
    case eR32G32Sfloat:
        return RG32_FLOAT;
    case eR32G32B32Uint:
        return RGB32_UINT;
    case eR32G32B32Sint:
        return RGB32_SINT;
    case eR32G32B32Sfloat:
        return RGB32_FLOAT;
    case eR32G32B32A32Uint:
        return RGBA32_UINT;
    case eR32G32B32A32Sint:
        return RGBA32_SINT;
    case eR32G32B32A32Sfloat:
        return RGBA32_FLOAT;

    // Packed formats
    case eA2B10G10R10UnormPack32:
        return RGB10A2_UNORM; // Note format difference
    case eA2B10G10R10UintPack32:
        return RGB10A2_UINT; // Note format difference
    case eB10G11R11UfloatPack32:
        return RG11B10_FLOAT; // Note format difference

    // Depth/stencil formats
    case eD16Unorm:
        return D16_UNORM;
    case eD24UnormS8Uint:
        return D24_UNORM_S8_UINT;
    case eD32Sfloat:
        return D32_FLOAT;
    case eD32SfloatS8Uint:
        return D32_FLOAT_S8_UINT;

    // BC compressed formats
    case eBc1RgbaUnormBlock:
    case eBc1RgbaSrgbBlock:
        return BC1_UNORM;
    case eBc2UnormBlock:
    case eBc2SrgbBlock:
        return BC2_UNORM;
    case eBc3UnormBlock:
    case eBc3SrgbBlock:
        return BC3_UNORM;
    case eBc4UnormBlock:
        return BC4_UNORM;
    case eBc4SnormBlock:
        return BC4_SNORM;
    case eBc5UnormBlock:
        return BC5_UNORM;
    case eBc5SnormBlock:
        return BC5_SNORM;
    case eBc6HUfloatBlock:
        return BC6H_UF16;
    case eBc6HSfloatBlock:
        return BC6H_SF16;
    case eBc7UnormBlock:
    case eBc7SrgbBlock:
        return BC7_UNORM;

    default:
        return UNKNOWN;
    }
}

} // namespace vex::vk