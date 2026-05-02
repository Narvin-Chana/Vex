#include "Texture.h"

#include <cmath>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Formattable.h>
#include <VexMacros.h>

namespace vex
{

namespace TextureUtil
{

u32 GetSubresourceIndex(const TextureDesc& desc, u16 mip, u32 slice, u32 plane)
{
    VEX_ASSERT(mip < desc.mips && slice < desc.GetSliceCount() && plane < desc.GetPlaneCount());
    return plane * (desc.mips * desc.GetSliceCount()) + static_cast<u32>(mip) + slice * desc.mips;
}

std::tuple<u32, u32, u32> GetMipSize(const TextureDesc& desc, u32 mip)
{
    VEX_ASSERT(mip < desc.mips);

    return { std::max(desc.width >> mip, 1u), std::max(desc.height >> mip, 1u), std::max(desc.GetDepth() >> mip, 1u) };
}

TextureViewType GetTextureViewType(const TextureDesc& desc, bool textureCubeAsTexture2DArray)
{
    if (textureCubeAsTexture2DArray && desc.type == TextureType::TextureCube)
    {
        return TextureViewType::Texture2DArray;
    }

    switch (desc.type)
    {
    case TextureType::Texture2D:
        return (desc.depthOrSliceCount > 1) ? TextureViewType::Texture2DArray : TextureViewType::Texture2D;
    case TextureType::TextureCube:
        return (desc.depthOrSliceCount > 1) ? TextureViewType::TextureCubeArray : TextureViewType::TextureCube;
    case TextureType::Texture3D:
        return TextureViewType::Texture3D;
    default:
        VEX_LOG(Fatal, "Unrecognized texture type for texture: {}.", desc.name);
    }
    std::unreachable();
}

TextureViewType GetTextureViewType(const TextureBinding& binding)
{
    return GetTextureViewType(binding.texture.desc, binding.textureCubeAsTexture2DArray);
}

TextureFormat GetCopyFormat(TextureFormat format, TextureAspect aspect)
{
    if (FormatUtil::IsDepthAndStencilFormat(format))
    {
        if (aspect == TextureAspect::Depth)
        {
            if (format == TextureFormat::D32_FLOAT_S8_UINT)
            {
                return TextureFormat::R32_FLOAT;
            }
            return TextureFormat::R32_UINT;
        }
        return TextureFormat::R8_UINT;
    }
    return format;
}

void ValidateTextureDescription(const TextureDesc& desc)
{
    VEX_CHECK(desc.width != 0,
              "Invalid Texture description for texture \"{}\": Cannot create a texture with a width of 0.",
              desc.name);
    VEX_CHECK(desc.height != 0,
              "Invalid Texture description for texture \"{}\": Cannot create a texture with a height of 0.",
              desc.name);
    VEX_CHECK(desc.GetDepth() != 0,
              "Invalid Texture description for texture \"{}\": Cannot create a texture with a depth of 0.",
              desc.name);
    VEX_CHECK(desc.GetSliceCount() != 0,
              "Invalid Texture description for texture \"{}\": Cannot create a texture with a slice count of 0.",
              desc.name);

    bool isDepthStencilFormat = FormatUtil::IsDepthOrDepthStencilFormat(desc.format);
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

    if (index >= std::to_underlying(RGBA8_UNORM) && index <= std::to_underlying(BGRA8_UNORM))
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

    if (index >= std::to_underlying(RGB32_UINT) && index <= std::to_underlying(RGB32_FLOAT))
    {
        return 12;
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

    if (index >= std::to_underlying(BC1_UNORM) && index <= std::to_underlying(BC1_UNORM))
    {
        return 0.5f;
    }

    if (index >= std::to_underlying(BC2_UNORM) && index <= std::to_underlying(BC3_UNORM))
    {
        return 1;
    }

    if (index >= std::to_underlying(BC4_UNORM) && index <= std::to_underlying(BC4_SNORM))
    {
        return 0.5f;
    }

    if (index >= std::to_underlying(BC5_UNORM) && index <= std::to_underlying(BC7_UNORM))
    {
        return 1;
    }

    VEX_ASSERT(false, "Cannot have 0 width pixel texture format....");
    return 0;
}

u32 TextureAspectToPlaneIndex(TextureAspect aspect)
{
    if (aspect == TextureAspect::Stencil)
    {
        return 1;
    }
    return 0;
}

Flags<TextureAspect> PlaneStartCountToTextureAspect(TextureFormat format, u32 startPlane, u32 planeCount)
{
    if (planeCount == 1 && FormatUtil::IsDepthAndStencilFormat(format))
    {
        if (startPlane == 0)
            return TextureAspect::Depth;
        if (startPlane == 1)
            return TextureAspect::Stencil;
    }

    return TextureAspect::All;
}

u64 ComputeAlignedUploadBufferByteSize(const TextureDesc& desc, Span<const TextureRegion> uploadRegions)
{
    const float pixelByteSize = GetPixelByteSizeFromFormat(desc.format);
    float totalSize = 0;

    for (const TextureRegion& region : uploadRegions)
    {
        const u32 sliceCount = region.subresource.GetSliceCount(desc);

        VEX_CHECK(region.subresource.startSlice + sliceCount <= desc.GetSliceCount(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.subresource.startSlice + sliceCount,
                  desc.GetSliceCount());

        for (u16 mip = region.subresource.startMip;
             mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
             ++mip)
        {
            const u32 width = region.extent.GetWidth(desc, mip);
            const u32 height = region.extent.GetHeight(desc, mip);
            const u32 depth = region.extent.GetDepth(desc, mip);

            // The staging buffer should have a row pitch alignment of 256 and mip alignment of 512 due to API
            // constraints.
            u64 regionSize = AlignUp<u64>(width * pixelByteSize, RowPitchAlignment) * height * depth;
            totalSize += AlignUp<u64>(regionSize, MipAlignment) * sliceCount;
        }
    }

    return static_cast<u64>(std::ceil(totalSize));
}

u64 ComputePackedTextureDataByteSize(const TextureDesc& desc, Span<const TextureRegion> uploadRegions)
{
    // Pixel byte size could be less than 1 (BlockCompressed formats).
    float totalSize = 0;

    for (const TextureRegion& region : uploadRegions)
    {
        const float pixelByteSize =
            GetPixelByteSizeFromFormat(GetCopyFormat(desc.format, region.subresource.GetSingleAspect(desc)));

        const u32 sliceCount = region.subresource.GetSliceCount(desc);

        VEX_CHECK(region.subresource.startSlice + sliceCount <= desc.GetSliceCount(),
                  "Cannot upload to a slice index ({}) greater or equal to the the texture's slice count ({})!",
                  region.subresource.startSlice + sliceCount,
                  desc.GetSliceCount());

        for (u16 mip = region.subresource.startMip;
             mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
             ++mip)
        {
            const u32 width = region.extent.GetWidth(desc, mip);
            const u32 height = region.extent.GetHeight(desc, mip);
            const u32 depth = region.extent.GetDepth(desc, mip);

            // Calculate tightly packed size for this region
            totalSize += width * pixelByteSize * height * depth * sliceCount;
        }
    }

    return static_cast<u64>(std::ceil(totalSize));
}

bool IsBindingUsageCompatibleWithUsage(Flags<TextureUsage> usages, TextureBindingUsage bindingUsage)
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

void ForEachSubresourceIndices(const TextureSubresource& subresource,
                               const TextureDesc& desc,
                               std::function<void(u16 mip, u32 slice, u32 plane)> func)
{
    for (u16 mip = subresource.startMip; mip < subresource.startMip + subresource.GetMipCount(desc); ++mip)
    {
        for (u32 slice = subresource.startSlice; slice < subresource.startSlice + subresource.GetSliceCount(desc);
             ++slice)
        {
            for (u32 plane = subresource.GetStartPlane();
                 plane < subresource.GetStartPlane() + subresource.GetPlaneCount(desc);
                 ++plane)
            {
                func(mip, slice, plane);
            }
        }
    }
}

void ValidateSubresource(const TextureDesc& desc, const TextureSubresource& subresource)
{
    VEX_CHECK(subresource.startMip < desc.mips,
              "Invalid subresource for resource \"{}\": The subresource's startMip ({}) cannot be larger than the "
              "actual texture's mip count ({}).",
              desc.name,
              subresource.startMip,
              desc.mips);

    if (subresource.mipCount != GTextureAllMips)
    {
        VEX_CHECK(
            subresource.startMip + subresource.GetMipCount(desc) <= desc.mips,
            "Invalid subresource for resource \"{}\": TextureSubresource accesses more mips than available, startMip : "
            "{}, mipCount: "
            "{}, texture mip count: {}",
            desc.name,
            subresource.startMip,
            subresource.GetMipCount(desc),
            desc.mips);
    }

    VEX_CHECK(
        subresource.startSlice < desc.GetSliceCount(),
        "Invalid subresource for resource \"{}\": The subresource's starting slice ({}) cannot be larger than the "
        "actual texture's array size ({}).",
        desc.name,
        subresource.startSlice,
        desc.GetSliceCount());

    if (subresource.sliceCount != GTextureAllSlices)
    {
        VEX_CHECK(subresource.startSlice + subresource.sliceCount <= desc.GetSliceCount(),
                  "Invalid subresource for resource \"{}\": The subresource accesses more slices than available, "
                  "startSlice: {}, sliceCount: {}, "
                  " texture slice count {}",
                  desc.name,
                  subresource.startSlice,
                  subresource.sliceCount,
                  desc.GetSliceCount());
    }

    if (subresource.aspect != TextureAspect::All)
    {
        if (FormatUtil::IsDepthOrDepthStencilFormat(desc.format))
        {
            VEX_CHECK((subresource.aspect & TextureAspect::Color) == 0,
                      "Invalid subresource for resource \"{}\": Cannot use color aspect for depth stencil resource.",
                      desc.name);
        }
        else
        {
            VEX_CHECK((subresource.aspect & TextureAspect::Color) == TextureAspect::Color,
                      "Invalid subresource for resource \"{}\": Cannot use depth/stencil aspects for a color resource.",
                      desc.name);
        }

        if (FormatUtil::IsDepthOnlyFormat(desc.format))
        {
            VEX_CHECK((subresource.aspect & TextureAspect::Stencil) == 0,
                      "Invalid subresource for resource \"{}\": Cannot use stencil aspect for depth-only resource.",
                      desc.name);
        }
    }
}

void ValidateRegion(const TextureDesc& desc, const TextureRegion& region)
{
    ValidateSubresource(desc, region.subresource);

    // If any of the extents is not set to GTextureExtentMax, we validate that the underlying subresource only has 1
    // mip! (this requires a span of regions, since the extent is only valid for one mip).
    if (region.extent.width != GTextureExtentMax || region.extent.height != GTextureExtentMax ||
        region.extent.depth != GTextureExtentMax)
    {
        VEX_CHECK(region.subresource.GetMipCount(desc) == 1,
                  "Invalid region for resource \"{}\": If you use a non-default region extent, your region may only "
                  "describe a single mip.",
                  desc.name);
    }

    auto [mipWidth, mipHeight, mipDepth] = TextureUtil::GetMipSize(desc, region.subresource.startMip);
    for (u32 mip = region.subresource.startMip; mip < region.subresource.GetMipCount(desc); ++mip)
    {
        VEX_CHECK(region.offset.x < mipWidth && region.offset.y < mipHeight && region.offset.z < mipDepth,
                  "Invalid region for resource \"{}\": Region offset is beyond the mip's resource size. Mip size: "
                  "{}x{}x{}, region offset: {}x{}x{}",
                  desc.name,
                  mipWidth,
                  mipHeight,
                  mipDepth,
                  region.offset.x,
                  region.offset.y,
                  region.offset.z);

        const u32 offsetExtentWidth =
            (region.extent.width != GTextureExtentMax) ? region.offset.x + region.extent.width : mipWidth;
        const u32 offsetExtentHeight =
            (region.extent.height != GTextureExtentMax) ? region.offset.y + region.extent.height : mipHeight;
        const u32 offsetExtentDepth =
            (region.extent.depth != GTextureExtentMax) ? region.offset.z + region.extent.depth : mipDepth;
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

void ValidateCopyDesc(const TextureDesc& srcDesc, const TextureDesc& dstDesc, const TextureCopyDesc& copyDesc)
{
    ValidateRegion(srcDesc, copyDesc.srcRegion);
    ValidateRegion(dstDesc, copyDesc.dstRegion);
    VEX_CHECK(copyDesc.srcRegion.extent == copyDesc.dstRegion.extent,
              "A texture copy's src and dst extents should match!");
}

void ValidateCompatibleTextureDescs(const TextureDesc& srcDesc, const TextureDesc& dstDesc)
{
    VEX_CHECK(srcDesc.depthOrSliceCount == dstDesc.depthOrSliceCount && srcDesc.width == dstDesc.width &&
                  srcDesc.height == dstDesc.height && srcDesc.mips == dstDesc.mips &&
                  srcDesc.format == dstDesc.format && srcDesc.type == dstDesc.type,
              "Textures must have the same width, height, depth/array size, mips, format and type to be able to do a "
              "simple copy")
}

} // namespace TextureUtil

TextureDesc TextureDesc::CreateTexture2DDesc(std::string name,
                                             TextureFormat format,
                                             u32 width,
                                             u32 height,
                                             u16 mips,
                                             Flags<TextureUsage> usage,
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

TextureDesc TextureDesc::CreateTexture2DArrayDesc(std::string name,
                                                  TextureFormat format,
                                                  u32 width,
                                                  u32 height,
                                                  u32 arraySize,
                                                  u16 mips,
                                                  Flags<TextureUsage> usage,
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

TextureDesc TextureDesc::CreateTextureCubeDesc(std::string name,
                                               TextureFormat format,
                                               u32 faceSize,
                                               u16 mips,
                                               Flags<TextureUsage> usage,
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

TextureDesc TextureDesc::CreateTextureCubeArrayDesc(std::string name,
                                                    TextureFormat format,
                                                    u32 faceSize,
                                                    u32 arraySize,
                                                    u16 mips,
                                                    Flags<TextureUsage> usage,
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

TextureDesc TextureDesc::CreateTexture3DDesc(std::string name,
                                             TextureFormat format,
                                             u32 width,
                                             u32 height,
                                             u32 depth,
                                             u16 mips,
                                             Flags<TextureUsage> usage,
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

u32 TextureClearRect::GetExtentX(const TextureDesc& desc) const
{
    return extentX == GTextureClearRectMax ? desc.width - offsetX : extentX;
}

u32 TextureClearRect::GetExtentY(const TextureDesc& desc) const
{
    return extentY == GTextureClearRectMax ? desc.height - offsetY : extentY;
}

u16 TextureSubresource::GetMipCount(const TextureDesc& desc) const
{
    return mipCount == GTextureAllMips ? (desc.mips - startMip) : mipCount;
}

u32 TextureSubresource::GetSliceCount(const TextureDesc& desc) const
{
    return sliceCount == GTextureAllSlices ? (desc.GetSliceCount() - startSlice) : sliceCount;
}

u32 TextureSubresource::GetStartPlane() const
{
    if (aspect == TextureAspect::All)
    {
        return 0;
    }

    if (aspect & TextureAspect::Color || aspect & TextureAspect::Depth)
    {
        return 0;
    }
    return aspect & TextureAspect::Stencil ? 1 : 0;
}

u32 TextureSubresource::GetPlaneCount(const TextureDesc& desc) const
{
    if (aspect == TextureAspect::All)
    {
        return FormatUtil::GetPlaneCount(desc.format);
    }

    u32 count = 0;
    if (aspect & TextureAspect::Stencil)
    {
        count++;
    }

    if (aspect & TextureAspect::Color || aspect & TextureAspect::Depth)
    {
        count++;
    }
    return count;
}

Flags<TextureAspect> TextureSubresource::GetAspect(const TextureDesc& desc) const
{
    const bool isDepthOrDepthStencil = FormatUtil::IsDepthOrDepthStencilFormat(desc.format);
    const bool isDepthOnly = FormatUtil::IsDepthOnlyFormat(desc.format);

    Flags cleanAspect = TextureAspect::None;
    if (aspect & TextureAspect::Color && !isDepthOrDepthStencil)
        cleanAspect |= TextureAspect::Color;
    if (aspect & TextureAspect::Depth && isDepthOrDepthStencil)
        cleanAspect |= TextureAspect::Depth;
    if (aspect & TextureAspect::Stencil && isDepthOrDepthStencil && !isDepthOnly)
        cleanAspect |= TextureAspect::Stencil;
    return cleanAspect;
}

TextureAspect TextureSubresource::GetSingleAspect(const TextureDesc& desc) const
{
    if (aspect == TextureAspect::All)
    {
        return static_cast<TextureAspect>(GetDefaultAspect(desc).data);
    }
    VEX_ASSERT((aspect & (aspect - 1)) == 0, "Tried to get single aspect when multiple were flagged");
    return static_cast<TextureAspect>(aspect.data);
}

Flags<TextureAspect> TextureSubresource::GetDefaultAspect(const TextureDesc& desc)
{
    if (FormatUtil::IsDepthOrDepthStencilFormat(desc.format))
    {
        return TextureAspect::Depth;
    }
    return TextureAspect::Color;
}

bool TextureSubresource::IsFullResource(const TextureDesc& desc) const
{
    return startMip == 0 && (mipCount == GTextureAllMips || mipCount == desc.mips) && startSlice == 0 &&
           (sliceCount == GTextureAllSlices || sliceCount == desc.depthOrSliceCount) &&
           desc.GetPlaneCount() == GetPlaneCount(desc);
}

std::tuple<u32, u32, u32> TextureRegion::GetExtents(const TextureDesc& desc, u16 mip) const
{
    return { extent.GetWidth(desc, mip), extent.GetHeight(desc, mip), extent.GetDepth(desc, mip) };
}

TextureRegion TextureRegion::AllMips(Flags<TextureAspect> aspect)
{
    // Defaults will specify all mips and all slices.
    return TextureRegion{ .subresource{ .aspect = aspect } };
}

TextureRegion TextureRegion::SingleMip(u16 mipIndex, Flags<TextureAspect> aspect)
{
    return TextureRegion{ .subresource = { .startMip = mipIndex, .mipCount = 1, .aspect = aspect } };
}

u32 TextureExtent3D::GetWidth(const TextureDesc& desc, u16 mipIndex) const
{
    VEX_CHECK(mipIndex < desc.mips, "Cannot obtain the size of a mip that this texture does not possess.");
    return std::max(1u, (width == GTextureExtentMax ? desc.width : width) >> mipIndex);
}

u32 TextureExtent3D::GetHeight(const TextureDesc& desc, u16 mipIndex) const
{
    VEX_CHECK(mipIndex < desc.mips, "Cannot obtain the size of a mip that this texture does not possess.");
    return std::max(1u, (height == GTextureExtentMax ? desc.height : height) >> mipIndex);
}

u32 TextureExtent3D::GetDepth(const TextureDesc& desc, u16 mipIndex) const
{
    VEX_CHECK(mipIndex < desc.mips, "Cannot obtain the size of a mip that this texture does not possess.");
    return std::max(1u, (depth == GTextureExtentMax ? desc.GetDepth() : depth) >> mipIndex);
}

} // namespace vex
