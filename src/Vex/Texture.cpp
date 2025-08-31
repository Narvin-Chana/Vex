#include "Bindings.h"
#include "Logger.h"
#include "Texture.h"

namespace vex
{

namespace TextureUtil
{

TextureViewType GetTextureViewType(const TextureBinding& binding)
{
    switch (binding.texture.description.type)
    {
    case TextureType::Texture2D:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::Texture2DArray
                                                                  : TextureViewType::Texture2D;
    case TextureType::TextureCube:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::TextureCubeArray
                                                                  : TextureViewType::TextureCube;
    case TextureType::Texture3D:
        return TextureViewType::Texture3D;
    default:
        VEX_LOG(Fatal, "Unrecognized texture type...");
    }
    std::unreachable();
}

TextureFormat GetTextureFormat(const TextureBinding& binding)
{
    if (!IsFormatSRGB(binding.texture.description.format) &&
        !FormatHasSRGBEquivalent(binding.texture.description.format))
    {
        return TextureFormat::UNKNOWN;
    }

    if (binding.flags & TextureBindingFlags::SRGB)
    {
        return GetSRGBEquivalentFormat(binding.texture.description.format);
    }

    return binding.texture.description.format;
}

void ValidateTextureDescription(const TextureDescription& description)
{
    bool isDepthStencilFormat = FormatIsDepthStencilCompatible(description.format);
    if (isDepthStencilFormat && !(description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a DepthStencil usage must have a "
                "format that supports it",
                description.name);
    }

    if (!isDepthStencilFormat && (description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a non-depth-stencil compatible "
                "format must allow for DepthStencil usage.",
                description.name);
    }

    if ((description.usage & TextureUsage::RenderTarget) && (description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture cannot have both RenderTarget AND "
                "DepthStencil usage.",
                description.name);
    }
}

u8 GetPixelByteSizeFromFormat(TextureFormat format)
{
    using enum TextureFormat;

    u8 index = std::to_underlying(format);

    if (index >= std::to_underlying(R8_UNORM) && index <= std::to_underlying(R8_SINT))
    {
        return 1;
    }

    if (index >= std::to_underlying(RG8_UNORM) && index <= std::to_underlying(RG8_SINT))
    {
        return 2;
    }

    if (index >= std::to_underlying(RGBA8_UNORM) && index <= std::to_underlying(BGRA8_UNORM_SRGB))
    {
        return 4;
    }

    if (index >= std::to_underlying(R16_UINT) && index <= std::to_underlying(R16_FLOAT))
    {
        return 2;
    }

    if (index >= std::to_underlying(RG16_UINT) && index <= std::to_underlying(RG16_FLOAT))
    {
        return 4;
    }

    if (index >= std::to_underlying(RGBA16_UINT) && index <= std::to_underlying(RGBA16_FLOAT))
    {
        return 8;
    }

    if (index == std::to_underlying(D16_UNORM))
    {
        return 2;
    }

    if (index == std::to_underlying(D24_UNORM_S8_UINT) || index == std::to_underlying(D32_FLOAT))
    {
        return 4;
    }

    if (index == std::to_underlying(D32_FLOAT_S8_UINT))
    {
        return 5;
    }

    if (index >= std::to_underlying(RGB10A2_UNORM) && index <= std::to_underlying(RG11B10_FLOAT))
    {
        return 4;
    }

    // TODO: handle compression formats
    VEX_LOG(Fatal, "Format size not handled")
    return 0;
}

u32 GetTotalTextureByteSize(const TextureDescription& desc)
{
    u32 pixelByteSize = GetPixelByteSizeFromFormat(desc.format);

    u32 totalSize = 0;
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.type == TextureType::Texture3D ? desc.depthOrArraySize : 1;
    u32 arraySize = desc.type != TextureType::Texture3D ? desc.depthOrArraySize : 1;

    for (int i = 0; i < desc.mips; ++i)
    {
        totalSize += width * height * depth * arraySize * pixelByteSize;

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }
    return totalSize;
}

bool IsTextureBindingUsageCompatibleWithTextureUsage(TextureUsage::Flags usages, TextureBindingUsage bindingUsage)
{
    if (bindingUsage == TextureBindingUsage::ShaderRead)
    {
        return usages & TextureUsage::ShaderRead;
    }

    if (bindingUsage == TextureBindingUsage::ShaderReadWrite)
    {
        return usages & TextureUsage::ShaderReadWrite;
    }

    return true;
}

} // namespace TextureUtil

} // namespace vex
