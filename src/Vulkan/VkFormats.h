#pragma once

#include <utility>

#include <vulkan/vulkan_core.h>

#include <Vex/Formats.h>

namespace vex::vk
{

// Certain formats require special care because they differ from DX12 to Vulkan in terms of component ordering.
// This method returns true if the format has ordering different to what is found in vex::TextureFormat.
constexpr inline bool IsSpecialFormat(VkFormat format)
{
    // Packed formats
    switch (format)
    {
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return true;
    default:
        return false;
    }
    std::unreachable();
}

// Convert from TextureFormat to VkFormat
constexpr ::vk::Format TextureFormatToVulkan(TextureFormat format)
{
    using enum ::vk::Format;
    switch (format)
    {
    // Standard formats
    case TextureFormat::R8_UNORM:
        return eR8Unorm;
    case TextureFormat::R8_SNORM:
        return eR8Snorm;
    case TextureFormat::R8_UINT:
        return eR8Uint;
    case TextureFormat::R8_SINT:
        return eR8Sint;
    case TextureFormat::RG8_UNORM:
        return eR8G8Unorm;
    case TextureFormat::RG8_SNORM:
        return eR8G8Snorm;
    case TextureFormat::RG8_UINT:
        return eR8G8Uint;
    case TextureFormat::RG8_SINT:
        return eR8G8Sint;
    case TextureFormat::RGBA8_UNORM:
        return eR8G8B8A8Unorm;
    case TextureFormat::RGBA8_UNORM_SRGB:
        return eR8G8B8A8Srgb;
    case TextureFormat::RGBA8_SNORM:
        return eR8G8B8A8Snorm;
    case TextureFormat::RGBA8_UINT:
        return eR8G8B8A8Uint;
    case TextureFormat::RGBA8_SINT:
        return eR8G8B8A8Sint;
    case TextureFormat::BGRA8_UNORM:
        return eB8G8R8A8Unorm;
    case TextureFormat::BGRA8_UNORM_SRGB:
        return eB8G8R8A8Srgb;

    // 16-bit formats
    case TextureFormat::R16_UINT:
        return eR16Uint;
    case TextureFormat::R16_SINT:
        return eR16Sint;
    case TextureFormat::R16_FLOAT:
        return eR16Sfloat;
    case TextureFormat::RG16_UINT:
        return eR16G16Uint;
    case TextureFormat::RG16_SINT:
        return eR16G16Sint;
    case TextureFormat::RG16_FLOAT:
        return eR16G16Sfloat;
    case TextureFormat::RGBA16_UINT:
        return eR16G16B16A16Uint;
    case TextureFormat::RGBA16_SINT:
        return eR16G16B16A16Sint;
    case TextureFormat::RGBA16_FLOAT:
        return eR16G16B16A16Sfloat;

    // 32-bit formats
    case TextureFormat::R32_UINT:
        return eR32Uint;
    case TextureFormat::R32_SINT:
        return eR32Sint;
    case TextureFormat::R32_FLOAT:
        return eR32Sfloat;
    case TextureFormat::RG32_UINT:
        return eR32G32Uint;
    case TextureFormat::RG32_SINT:
        return eR32G32Sint;
    case TextureFormat::RG32_FLOAT:
        return eR32G32Sfloat;
    case TextureFormat::RGB32_UINT:
        return eR32G32B32Uint;
    case TextureFormat::RGB32_SINT:
        return eR32G32B32Sint;
    case TextureFormat::RGB32_FLOAT:
        return eR32G32B32Sfloat;
    case TextureFormat::RGBA32_UINT:
        return eR32G32B32A32Uint;
    case TextureFormat::RGBA32_SINT:
        return eR32G32B32A32Sint;
    case TextureFormat::RGBA32_FLOAT:
        return eR32G32B32A32Sfloat;

    // Packed formats
    case TextureFormat::RGB10A2_UNORM:
        return eA2R10G10B10UnormPack32; // Note format difference
    case TextureFormat::RGB10A2_UINT:
        return eA2R10G10B10UintPack32; // Note format difference
    case TextureFormat::RG11B10_FLOAT:
        return eB10G11R11UfloatPack32; // Note format difference

    // Depth/stencil formats
    case TextureFormat::D16_UNORM:
        return eD16Unorm;
    case TextureFormat::D24_UNORM_S8_UINT:
        return eD24UnormS8Uint;
    case TextureFormat::D32_FLOAT:
        return eD32Sfloat;
    case TextureFormat::D32_FLOAT_S8_UINT:
        return eD32SfloatS8Uint;

    // BC compressed formats
    case TextureFormat::BC1_UNORM:
        return eBc1RgbaUnormBlock;
    case TextureFormat::BC1_UNORM_SRGB:
        return eBc1RgbaSrgbBlock;
    case TextureFormat::BC2_UNORM:
        return eBc2UnormBlock;
    case TextureFormat::BC2_UNORM_SRGB:
        return eBc2SrgbBlock;
    case TextureFormat::BC3_UNORM:
        return eBc3UnormBlock;
    case TextureFormat::BC3_UNORM_SRGB:
        return eBc3SrgbBlock;
    case TextureFormat::BC4_UNORM:
        return eBc4UnormBlock;
    case TextureFormat::BC4_SNORM:
        return eBc4SnormBlock;
    case TextureFormat::BC5_UNORM:
        return eBc5UnormBlock;
    case TextureFormat::BC5_SNORM:
        return eBc5SnormBlock;
    case TextureFormat::BC6H_UF16:
        return eBc6HUfloatBlock;
    case TextureFormat::BC6H_SF16:
        return eBc6HSfloatBlock;
    case TextureFormat::BC7_UNORM:
        return eBc7UnormBlock;
    case TextureFormat::BC7_UNORM_SRGB:
        return eBc7SrgbBlock;

    default:
        return eUndefined;
    }
}

// Convert from VkFormat to TextureFormat
constexpr inline TextureFormat VulkanToTextureFormat(::vk::Format format)
{
    switch (static_cast<VkFormat>(format))
    {
    // Standard formats
    case VK_FORMAT_R8_UNORM:
        return TextureFormat::R8_UNORM;
    case VK_FORMAT_R8_SNORM:
        return TextureFormat::R8_SNORM;
    case VK_FORMAT_R8_UINT:
        return TextureFormat::R8_UINT;
    case VK_FORMAT_R8_SINT:
        return TextureFormat::R8_SINT;
    case VK_FORMAT_R8G8_UNORM:
        return TextureFormat::RG8_UNORM;
    case VK_FORMAT_R8G8_SNORM:
        return TextureFormat::RG8_SNORM;
    case VK_FORMAT_R8G8_UINT:
        return TextureFormat::RG8_UINT;
    case VK_FORMAT_R8G8_SINT:
        return TextureFormat::RG8_SINT;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return TextureFormat::RGBA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return TextureFormat::RGBA8_UNORM_SRGB;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return TextureFormat::RGBA8_SNORM;
    case VK_FORMAT_R8G8B8A8_UINT:
        return TextureFormat::RGBA8_UINT;
    case VK_FORMAT_R8G8B8A8_SINT:
        return TextureFormat::RGBA8_SINT;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return TextureFormat::BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return TextureFormat::BGRA8_UNORM_SRGB;

    // 16-bit formats
    case VK_FORMAT_R16_UINT:
        return TextureFormat::R16_UINT;
    case VK_FORMAT_R16_SINT:
        return TextureFormat::R16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return TextureFormat::R16_FLOAT;
    case VK_FORMAT_R16G16_UINT:
        return TextureFormat::RG16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return TextureFormat::RG16_SINT;
    case VK_FORMAT_R16G16_SFLOAT:
        return TextureFormat::RG16_FLOAT;
    case VK_FORMAT_R16G16B16A16_UINT:
        return TextureFormat::RGBA16_UINT;
    case VK_FORMAT_R16G16B16A16_SINT:
        return TextureFormat::RGBA16_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return TextureFormat::RGBA16_FLOAT;

    // 32-bit formats
    case VK_FORMAT_R32_UINT:
        return TextureFormat::R32_UINT;
    case VK_FORMAT_R32_SINT:
        return TextureFormat::R32_SINT;
    case VK_FORMAT_R32_SFLOAT:
        return TextureFormat::R32_FLOAT;
    case VK_FORMAT_R32G32_UINT:
        return TextureFormat::RG32_UINT;
    case VK_FORMAT_R32G32_SINT:
        return TextureFormat::RG32_SINT;
    case VK_FORMAT_R32G32_SFLOAT:
        return TextureFormat::RG32_FLOAT;
    case VK_FORMAT_R32G32B32_UINT:
        return TextureFormat::RGB32_UINT;
    case VK_FORMAT_R32G32B32_SINT:
        return TextureFormat::RGB32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return TextureFormat::RGB32_FLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return TextureFormat::RGBA32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:
        return TextureFormat::RGBA32_SINT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return TextureFormat::RGBA32_FLOAT;

    // Packed formats
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return TextureFormat::RGB10A2_UNORM; // Note format difference
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        return TextureFormat::RGB10A2_UINT; // Note format difference
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return TextureFormat::RG11B10_FLOAT; // Note format difference

    // Depth/stencil formats
    case VK_FORMAT_D16_UNORM:
        return TextureFormat::D16_UNORM;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return TextureFormat::D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT:
        return TextureFormat::D32_FLOAT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return TextureFormat::D32_FLOAT_S8_UINT;

    // BC compressed formats
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return TextureFormat::BC1_UNORM;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return TextureFormat::BC1_UNORM_SRGB;
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return TextureFormat::BC2_UNORM;
    case VK_FORMAT_BC2_SRGB_BLOCK:
        return TextureFormat::BC2_UNORM_SRGB;
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return TextureFormat::BC3_UNORM;
    case VK_FORMAT_BC3_SRGB_BLOCK:
        return TextureFormat::BC3_UNORM_SRGB;
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return TextureFormat::BC4_UNORM;
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return TextureFormat::BC4_SNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return TextureFormat::BC5_UNORM;
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return TextureFormat::BC5_SNORM;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        return TextureFormat::BC6H_UF16;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return TextureFormat::BC6H_SF16;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return TextureFormat::BC7_UNORM;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return TextureFormat::BC7_UNORM_SRGB;

    default:
        return TextureFormat::UNKNOWN;
    }
}

} // namespace vex::vk