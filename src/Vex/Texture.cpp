#include "Texture.h"

#include <cmath>

#include <Vex/Bindings.h>
#include <Vex/ByteUtils.h>
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

u64 ComputeAlignedUploadBufferByteSize(const TextureDescription& desc,
                                       std::span<const TextureUploadRegion> uploadRegions)
{
    const float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);
    float totalSize = 0;

    for (const TextureUploadRegion& region : uploadRegions)
    {
        VEX_CHECK(region.slice < desc.GetArraySize(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.slice,
                  desc.GetArraySize());

        const u32 width = region.extent.width;
        const u32 height = region.extent.height;
        const u32 depth = region.extent.depth;

        // The staging buffer should have a row pitch alignment of 256 and mip alignment of 512 due to API constraints.
        u64 regionSize = AlignUp<u64>(width * pixelByteSize, RowPitchAlignment) * height * depth;
        totalSize += AlignUp<u64>(regionSize, MipAlignment);
    }

    return static_cast<u64>(std::ceil(totalSize));
}

u64 ComputePackedUploadDataByteSize(const TextureDescription& desc, std::span<const TextureUploadRegion> uploadRegions)
{
    // Pixel byte size could be less than 1 (BlockCompressed formats).
    const float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);
    float totalSize = 0;

    for (const TextureUploadRegion& region : uploadRegions)
    {
        VEX_CHECK(region.slice < desc.GetArraySize(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.slice,
                  desc.GetArraySize());

        const u32 width = region.extent.width;
        const u32 height = region.extent.height;
        const u32 depth = region.extent.depth;

        // Calculate tightly packed size for this region
        totalSize += width * pixelByteSize * height * depth;
    }

    return static_cast<u64>(std::ceil(totalSize));
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

    VEX_CHECK(subresource.startSlice < description.GetArraySize(),
              "Subresource start slice is greater than texture's array size. Start slice: {}, array size: {}",
              subresource.startSlice,
              description.GetArraySize());

    VEX_CHECK(
        subresource.startSlice + subresource.sliceCount <= description.GetArraySize(),
        "Subresource accesses more slices than available. Start slice: {}, slice count: {}, texture array size: {}",
        subresource.startSlice,
        subresource.sliceCount,
        description.GetArraySize());

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
    ValidateTextureSubresource(srcDesc, copyDesc.srcSubresource);
    ValidateTextureSubresource(dstDesc, copyDesc.dstSubresource);
    ValidateTextureExtent(srcDesc, copyDesc.srcSubresource, copyDesc.extent);
    ValidateTextureExtent(dstDesc, copyDesc.dstSubresource, copyDesc.extent);
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

std::vector<TextureUploadRegion> TextureUploadRegion::UploadAllMips(const TextureDescription& textureDescription)
{
    const u32 arraySize = textureDescription.GetArraySize();
    std::vector<TextureUploadRegion> regions(textureDescription.mips * arraySize);

    u32 width = textureDescription.width;
    u32 height = textureDescription.height;
    u32 depth = textureDescription.GetDepth();

    for (u16 mip = 0; mip < textureDescription.mips; ++mip)
    {
        for (u32 slice = 0; slice < arraySize; ++slice)
        {
            regions[mip * arraySize + slice] = {
                .mip = mip,
                .slice = slice,
                .offset = { 0, 0, 0 },
                .extent = { width, height, depth },
            };
        }

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        if (textureDescription.type == TextureType::Texture3D)
        {
            depth = std::max(1u, depth / 2u);
        }
    }

    return regions;
}

std::vector<TextureUploadRegion> TextureUploadRegion::UploadFullMip(u16 mipIndex,
                                                                    const TextureDescription& textureDescription)
{
    const u32 arraySize = textureDescription.GetArraySize();
    std::vector<TextureUploadRegion> regions(arraySize);

    const u32 width = std::max(1u, textureDescription.width >> mipIndex);
    const u32 height = std::max(1u, textureDescription.height >> mipIndex);
    const u32 depth = std::max(1u, textureDescription.GetDepth() >> mipIndex);

    for (u32 slice = 0; slice < arraySize; ++slice)
    {
        regions[slice] = {
            .mip = mipIndex,
            .slice = slice,
            .offset = { 0, 0, 0 },
            .extent = { width, height, depth },
        };
    }

    return regions;
}

TextureDescription TextureDescription::CreateTexture2D(std::string name,
                                                       TextureFormat format,
                                                       u32 width,
                                                       u32 height,
                                                       u16 mips,
                                                       TextureUsage::Flags usage,
                                                       TextureClearValue clearValue,
                                                       ResourceMemoryLocality memoryLocality)
{
    TextureDescription description{
        .name = std::move(name),
        .type = TextureType::Texture2D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrArraySize = 1,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return description;
}

TextureDescription TextureDescription::CreateTexture2DArray(std::string name,
                                                            TextureFormat format,
                                                            u32 width,
                                                            u32 height,
                                                            u32 arraySize,
                                                            u16 mips,
                                                            TextureUsage::Flags usage,
                                                            TextureClearValue clearValue,
                                                            ResourceMemoryLocality memoryLocality)
{
    TextureDescription description{
        .name = std::move(name),
        .type = TextureType::Texture2D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrArraySize = arraySize,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return description;
}

TextureDescription TextureDescription::CreateTextureCube(std::string name,
                                                         TextureFormat format,
                                                         u32 faceSize,
                                                         u16 mips,
                                                         TextureUsage::Flags usage,
                                                         TextureClearValue clearValue,
                                                         ResourceMemoryLocality memoryLocality)
{
    TextureDescription description{
        .name = std::move(name),
        .type = TextureType::TextureCube,
        .format = format,
        .width = faceSize,
        .height = faceSize,
        .depthOrArraySize = 1,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return description;
}

TextureDescription TextureDescription::CreateTextureCubeArray(std::string name,
                                                              TextureFormat format,
                                                              u32 faceSize,
                                                              u32 arraySize,
                                                              u16 mips,
                                                              TextureUsage::Flags usage,
                                                              TextureClearValue clearValue,
                                                              ResourceMemoryLocality memoryLocality)
{
    TextureDescription description{
        .name = std::move(name),
        .type = TextureType::TextureCube,
        .format = format,
        .width = faceSize,
        .height = faceSize,
        .depthOrArraySize = arraySize,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return description;
}

TextureDescription TextureDescription::CreateTexture3D(std::string name,
                                                       TextureFormat format,
                                                       u32 width,
                                                       u32 height,
                                                       u32 depth,
                                                       u16 mips,
                                                       TextureUsage::Flags usage,
                                                       TextureClearValue clearValue,
                                                       ResourceMemoryLocality memoryLocality)
{
    TextureDescription description{
        .name = std::move(name),
        .type = TextureType::Texture3D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrArraySize = depth,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return description;
}

} // namespace vex
