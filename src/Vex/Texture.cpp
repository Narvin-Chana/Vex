#include "Texture.h"

#include <cmath>

#include <Vex/Bindings.h>
#include <Vex/Formattable.h>
#include <Vex/Logger.h>
#include <Vex/Validation.h>

namespace vex
{

namespace TextureUtil
{

std::tuple<u32, u32, u32> GetMipSize(const TextureDescription& desc, u32 mip)
{
    VEX_ASSERT(mip < desc.mips);

    return { std::max(desc.width >> mip, 1u), std::max(desc.height >> mip, 1u), std::max(desc.GetDepth() >> mip, 1u) };
}

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
    if (binding.flags & TextureBindingFlags::SRGB)
    {
        if (!IsFormatSRGB(binding.texture.description.format) ||
            !FormatHasSRGBEquivalent(binding.texture.description.format))
        {
            VEX_LOG(Fatal,
                    "Format {} cannot support SRGB loads. Please use an SRGB-compatible texture format.",
                    binding.texture.description.format);
        }

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

float GetPixelByteSizeFromFormat(TextureFormat format)
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

    if (index >= std::to_underlying(R32_UINT) && index <= std::to_underlying(R32_FLOAT))
    {
        return 4;
    }

    if (index >= std::to_underlying(RG32_UINT) && index <= std::to_underlying(RG32_FLOAT))
    {
        return 8;
    }

    if (index >= std::to_underlying(RGBA32_UINT) && index <= std::to_underlying(RGBA32_FLOAT))
    {
        return 16;
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

    if (index >= std::to_underlying(BC1_UNORM) && index <= std::to_underlying(BC1_UNORM_SRGB))
    {
        return 0.5f;
    }

    if (index >= std::to_underlying(BC2_UNORM) && index <= std::to_underlying(BC3_UNORM_SRGB))
    {
        return 1;
    }

    if (index >= std::to_underlying(BC4_UNORM) && index <= std::to_underlying(BC4_SNORM))
    {
        return 0.5f;
    }

    if (index >= std::to_underlying(BC5_UNORM) && index <= std::to_underlying(BC7_UNORM_SRGB))
    {
        return 1;
    }

    return 0;
}

u32 GetTotalTextureByteSize(const TextureDescription& desc)
{
    float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);

    float totalSize = 0;
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.GetDepth();
    u32 arraySize = desc.GetArrayCount();

    for (int i = 0; i < desc.mips; ++i)
    {
        totalSize += static_cast<float>(width * height * depth * arraySize) * pixelByteSize;

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }
    return static_cast<u32>(std::ceil(totalSize));
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

void ValidateTextureSubresource(const TextureDescription& description, const TextureSubresource& subresource)
{
    VEX_CHECK(
        subresource.mip < description.mips,
        "Validation failed: Subresource mip is greater than texture's mip count. Subresource mip: {}, texture copy: {}",
        subresource.mip,
        description.mips);

    VEX_CHECK(subresource.startSlice < description.GetArrayCount(),
              "Subresource start slice is greater than texture's array size. Start slice: {}, array size: {}",
              subresource.startSlice,
              description.GetArrayCount());

    VEX_CHECK(
        subresource.startSlice + subresource.sliceCount <= description.GetArrayCount(),
        "Subresource accesses more slice than available. Start slice: {}, slice count: {}, texture array size: {}",
        subresource.startSlice,
        subresource.sliceCount,
        description.GetArrayCount());

    const auto [mipWidth, mipHeight, mipDepth] = GetMipSize(description, subresource.mip);
    VEX_CHECK(subresource.offset.width < mipWidth && subresource.offset.height < mipHeight &&
                  subresource.offset.depth < mipDepth,
              "Subresource offset is beyond the mip's resource size. Mip size: {}x{}x{}, subresource offset: {}x{}x{}",
              mipWidth,
              mipHeight,
              mipDepth,
              subresource.offset.width,
              subresource.offset.height,
              subresource.offset.depth);
}

void ValidateTextureCopyDescription(const TextureDescription& srcDesc,
                                    const TextureDescription& dstDesc,
                                    const TextureCopyDescription& copyDesc)
{
    ValidateTextureSubresource(srcDesc, copyDesc.srcRegion);
    ValidateTextureSubresource(dstDesc, copyDesc.dstRegion);
    ValidateTextureExtent(srcDesc, copyDesc.srcRegion, copyDesc.extent);
    ValidateTextureExtent(dstDesc, copyDesc.dstRegion, copyDesc.extent);
}
void ValidateTextureExtent(const TextureDescription& description,
                           const TextureSubresource& subresource,
                           const TextureExtent& extent)
{
    const auto [mipWidth, mipHeight, mipDepth] = GetMipSize(description, subresource.mip);
    const auto offsetExtentWidth = subresource.offset.width + extent.width;
    const auto offsetExtentHeight = subresource.offset.height + extent.height;
    const auto offsetExtentDepth = subresource.offset.depth + extent.depth;
    VEX_CHECK(offsetExtentWidth <= mipWidth && offsetExtentHeight <= mipHeight && offsetExtentDepth <= mipDepth,
              "Copy description extent goes beyon mip size: Extent + offset: {}x{}x{}, mip size: {}x{}x{}",
              offsetExtentWidth,
              offsetExtentHeight,
              offsetExtentDepth,
              mipWidth,
              mipHeight,
              mipDepth);
}

void ValidateCompatibleTextureDescriptions(const TextureDescription& srcDesc, const TextureDescription& dstDesc)
{
    VEX_CHECK(srcDesc.depthOrArraySize == dstDesc.depthOrArraySize && srcDesc.width == dstDesc.width &&
                  srcDesc.height == dstDesc.height && srcDesc.mips == dstDesc.mips &&
                  srcDesc.format == dstDesc.format && srcDesc.type == dstDesc.type,
              "Textures must have the same width, height, depth/array size, mips, format and type to be able to do a "
              "simple copy")
}

} // namespace TextureUtil

} // namespace vex
