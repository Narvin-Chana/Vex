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

std::tuple<u32, u32, u32> GetMipSize(const TextureDesc& desc, u32 mip)
{
    VEX_ASSERT(mip < desc.mips);

    return { std::max(desc.width >> mip, 1u), std::max(desc.height >> mip, 1u), std::max(desc.GetDepth() >> mip, 1u) };
}

TextureViewType GetTextureViewType(const TextureBinding& binding)
{
    switch (binding.texture.desc.type)
    {
    case TextureType::Texture2D:
        return (binding.texture.desc.depthOrSliceCount > 1) ? TextureViewType::Texture2DArray
                                                            : TextureViewType::Texture2D;
    case TextureType::TextureCube:
        return (binding.texture.desc.depthOrSliceCount > 1) ? TextureViewType::TextureCubeArray
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
        if (!IsFormatSRGB(binding.texture.desc.format) || !FormatHasSRGBEquivalent(binding.texture.desc.format))
        {
            VEX_LOG(Fatal,
                    "Format {} cannot support SRGB loads. Please use an SRGB-compatible texture format.",
                    binding.texture.desc.format);
        }

        return GetSRGBEquivalentFormat(binding.texture.desc.format);
    }

    return binding.texture.desc.format;
}

void ValidateTextureDescription(const TextureDesc& desc)
{
    bool isDepthStencilFormat = FormatIsDepthStencilCompatible(desc.format);
    if (isDepthStencilFormat && !(desc.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a DepthStencil usage must have a "
                "format that supports it",
                desc.name);
    }

    if (!isDepthStencilFormat && (desc.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a non-depth-stencil compatible "
                "format must allow for DepthStencil usage.",
                desc.name);
    }

    if ((desc.usage & TextureUsage::RenderTarget) && (desc.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture cannot have both RenderTarget AND "
                "DepthStencil usage.",
                desc.name);
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

u64 ComputeAlignedUploadBufferByteSize(const TextureDesc& desc, std::span<const TextureUploadRegion> uploadRegions)
{
    const float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);
    float totalSize = 0;

    for (const TextureUploadRegion& region : uploadRegions)
    {
        VEX_CHECK(region.slice < desc.GetSliceCount(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.slice,
                  desc.GetSliceCount());

        const u32 width = region.extent.width;
        const u32 height = region.extent.height;
        const u32 depth = region.extent.depth;

        // The staging buffer should have a row pitch alignment of 256 and mip alignment of 512 due to API constraints.
        u64 regionSize = AlignUp<u64>(width * pixelByteSize, RowPitchAlignment) * height * depth;
        totalSize += AlignUp<u64>(regionSize, MipAlignment);
    }

    return static_cast<u64>(std::ceil(totalSize));
}

u64 ComputePackedUploadDataByteSize(const TextureDesc& desc, std::span<const TextureUploadRegion> uploadRegions)
{
    // Pixel byte size could be less than 1 (BlockCompressed formats).
    const float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);
    float totalSize = 0;

    for (const TextureUploadRegion& region : uploadRegions)
    {
        VEX_CHECK(region.slice < desc.GetSliceCount(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.slice,
                  desc.GetSliceCount());

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

void ValidateTextureCopyDesc(const TextureDesc& srcDesc, const TextureDesc& dstDesc, const TextureCopyDesc& copyDesc)
{
    copyDesc.srcRegion.Validate(srcDesc);
    copyDesc.dstRegion.Validate(dstDesc);
    VEX_CHECK(copyDesc.srcRegion.extent == copyDesc.dstRegion.extent, "");
}

void ValidateCompatibleTextureDescriptions(const TextureDesc& srcDesc, const TextureDesc& dstDesc)
{
    VEX_CHECK(srcDesc.depthOrSliceCount == dstDesc.depthOrSliceCount && srcDesc.width == dstDesc.width &&
                  srcDesc.height == dstDesc.height && srcDesc.mips == dstDesc.mips &&
                  srcDesc.format == dstDesc.format && srcDesc.type == dstDesc.type,
              "Textures must have the same width, height, depth/array size, mips, format and type to be able to do a "
              "simple copy")
}

} // namespace TextureUtil

std::vector<TextureUploadRegion> TextureUploadRegion::UploadAllMips(const TextureDesc& textureDescription)
{
    const u32 arraySize = textureDescription.GetSliceCount();
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

std::vector<TextureUploadRegion> TextureUploadRegion::UploadFullMip(u16 mipIndex, const TextureDesc& textureDescription)
{
    const u32 arraySize = textureDescription.GetSliceCount();
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

TextureDesc TextureDesc::CreateTexture2D(std::string name,
                                         TextureFormat format,
                                         u32 width,
                                         u32 height,
                                         u16 mips,
                                         TextureUsage::Flags usage,
                                         TextureClearValue clearValue,
                                         ResourceMemoryLocality memoryLocality)
{
    TextureDesc desc{
        .name = std::move(name),
        .type = TextureType::Texture2D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrSliceCount = 1,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return desc;
}

TextureDesc TextureDesc::CreateTexture2DArray(std::string name,
                                              TextureFormat format,
                                              u32 width,
                                              u32 height,
                                              u32 arraySize,
                                              u16 mips,
                                              TextureUsage::Flags usage,
                                              TextureClearValue clearValue,
                                              ResourceMemoryLocality memoryLocality)
{
    TextureDesc desc{
        .name = std::move(name),
        .type = TextureType::Texture2D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrSliceCount = arraySize,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return desc;
}

TextureDesc TextureDesc::CreateTextureCube(std::string name,
                                           TextureFormat format,
                                           u32 faceSize,
                                           u16 mips,
                                           TextureUsage::Flags usage,
                                           TextureClearValue clearValue,
                                           ResourceMemoryLocality memoryLocality)
{
    TextureDesc desc{
        .name = std::move(name),
        .type = TextureType::TextureCube,
        .format = format,
        .width = faceSize,
        .height = faceSize,
        .depthOrSliceCount = 1,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return desc;
}

TextureDesc TextureDesc::CreateTextureCubeArray(std::string name,
                                                TextureFormat format,
                                                u32 faceSize,
                                                u32 arraySize,
                                                u16 mips,
                                                TextureUsage::Flags usage,
                                                TextureClearValue clearValue,
                                                ResourceMemoryLocality memoryLocality)
{
    TextureDesc desc{
        .name = std::move(name),
        .type = TextureType::TextureCube,
        .format = format,
        .width = faceSize,
        .height = faceSize,
        .depthOrSliceCount = arraySize,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return desc;
}

TextureDesc TextureDesc::CreateTexture3D(std::string name,
                                         TextureFormat format,
                                         u32 width,
                                         u32 height,
                                         u32 depth,
                                         u16 mips,
                                         TextureUsage::Flags usage,
                                         TextureClearValue clearValue,
                                         ResourceMemoryLocality memoryLocality)
{
    TextureDesc desc{
        .name = std::move(name),
        .type = TextureType::Texture3D,
        .format = format,
        .width = width,
        .height = height,
        .depthOrSliceCount = depth,
        .mips = mips,
        .usage = usage,
        .clearValue = std::move(clearValue),
        .memoryLocality = memoryLocality,
    };
    return desc;
}

u16 TextureSubresource::GetMipCount(const TextureDesc& desc) const
{
    return mipCount == GTextureAllMips ? (desc.mips - startMip) : mipCount;
}

u32 TextureSubresource::GetSliceCount(const TextureDesc& desc) const
{
    return sliceCount == GTextureAllSlices ? (desc.GetSliceCount() - startSlice) : sliceCount;
}

void TextureSubresource::Validate(const TextureDesc& desc) const
{
    VEX_CHECK(startMip < desc.mips,
              "Invalid subresource for resource \"{}\": The subresource's startMip ({}) cannot be larger than the "
              "actual texture's mip count ({}).",
              desc.name,
              startMip,
              desc.mips);

    if (mipCount != GTextureAllMips)
    {
        VEX_CHECK(
            startMip + mipCount <= desc.mips,
            "Invalid subresource for resource \"{}\": TextureSubresource accesses more mips than available, startMip : "
            "{}, mipCount: "
            "{}, texture mip count: {}",
            desc.name,
            startMip,
            mipCount,
            desc.mips);
    }

    VEX_CHECK(
        startSlice < desc.GetSliceCount(),
        "Invalid subresource for resource \"{}\": The subresource's starting slice ({}) cannot be larger than the "
        "actual texture's array size ({}).",
        desc.name,
        startSlice,
        desc.GetSliceCount());

    if (sliceCount != GTextureAllSlices)
    {
        VEX_CHECK(startSlice + sliceCount <= desc.GetSliceCount(),
                  "Invalid subresource for resource \"{}\": The subresource accesses more slices than available, "
                  "startSlice: {}, sliceCount: {}, "
                  " texture slice count {}",
                  desc.name,
                  startSlice,
                  sliceCount,
                  desc.GetSliceCount());
    }
}

void TextureRegion::Validate(const TextureDesc& desc) const
{
    subresource.Validate(desc);

    // If any of the extents is not set to GTextureExtentMax, we validate that the underlying subresource only has 1
    // mip! (this requires a span of regions, since the extent is only valid for one mip).
    if (extent.width != GTextureExtentMax || extent.height != GTextureExtentMax || extent.depth != GTextureExtentMax)
    {
        VEX_CHECK(subresource.mipCount == 1,
                  "Invalid region for resource \"{}\": If you use a non-default region extent, your region may only "
                  "describe a single mip.",
                  desc.name);
    }

    auto [mipWidth, mipHeight, mipDepth] = TextureUtil::GetMipSize(desc, subresource.startMip);
    for (u32 mip = subresource.startMip; mip < subresource.mipCount; ++mip)
    {
        VEX_CHECK(offset.width < mipWidth && offset.height < mipHeight && offset.depth < mipDepth,
                  "Invalid region for resource \"{}\": Region offset is beyond the mip's resource size. Mip size: "
                  "{}x{}x{}, region offset: {}x{}x{}",
                  desc.name,
                  mipWidth,
                  mipHeight,
                  mipDepth,
                  offset.width,
                  offset.height,
                  offset.depth);

        const u32 offsetExtentWidth = (extent.width != GTextureExtentMax) ? offset.width + extent.width : mipWidth;
        const u32 offsetExtentHeight = (extent.height != GTextureExtentMax) ? offset.height + extent.height : mipHeight;
        const u32 offsetExtentDepth = (extent.depth != GTextureExtentMax) ? offset.depth + extent.depth : mipDepth;
        VEX_CHECK(offsetExtentWidth <= mipWidth && offsetExtentHeight <= mipHeight && offsetExtentDepth <= mipDepth,
                  "Invalid region for resource \"{}\": Region extent goes beyond mip {} size: Extent + offset: "
                  "{}x{}x{}, Mip size: {}x{}x{}",
                  desc.name,
                  mip,
                  offsetExtentWidth,
                  offsetExtentHeight,
                  offsetExtentDepth,
                  mipWidth,
                  mipHeight,
                  mipDepth);

        mipWidth = std::max(1u, mipWidth / 2u);
        mipHeight = std::max(1u, mipHeight / 2u);
        mipDepth = std::max(1u, mipDepth / 2u);
    }
}

TextureRegion TextureRegion::AllMips()
{
    // Defaults will specify all mips and all slices.
    return TextureRegion{};
}

TextureRegion TextureRegion::SingleMip(u16 mipIndex)
{
    return TextureRegion{ .subresource = { .startMip = mipIndex, .mipCount = 1 } };
}

u32 TextureExtent3D::GetWidth(const TextureDesc& desc) const
{
    return width == GTextureExtentMax ? desc.width : width;
}

u32 TextureExtent3D::GetHeight(const TextureDesc& desc) const
{
    return height == GTextureExtentMax ? desc.height : height;
}

u32 TextureExtent3D::GetDepth(const TextureDesc& desc) const
{
    return depth == GTextureExtentMax ? desc.GetDepth() : depth;
}

} // namespace vex
