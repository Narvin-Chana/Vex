#include "ResourceCopy.h"

#include <cmath>
#include <cstring>

#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Validation.h>

namespace vex
{

namespace TextureCopyUtil
{
void ValidateBufferTextureCopyDesc(const BufferDesc& srcDesc,
                                   const TextureDesc& dstDesc,
                                   const BufferTextureCopyDesc& copyDesc)
{
    BufferUtil::ValidateBufferRegion(srcDesc, copyDesc.bufferRegion);
    TextureUtil::ValidateRegion(dstDesc, copyDesc.textureRegion);
}

void ReadTextureDataAligned(const TextureDesc& desc,
                            Span<const TextureRegion> textureRegions,
                            Span<const byte> alignedTextureData,
                            Span<byte> packedOutputData)

{
    const u32 bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(desc.format);
    const byte* srcData = alignedTextureData.data();
    byte* dstData = packedOutputData.data();
    u64 srcOffset = 0;
    u64 dstOffset = 0;

    for (const auto& region : textureRegions)
    {
        for (u16 mip = region.subresource.startMip;
             mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
             ++mip)
        {
            const u32 mipWidth = region.extent.GetWidth(desc, mip);
            const u32 mipHeight = region.extent.GetHeight(desc, mip);
            const u32 mipDepth = region.extent.GetDepth(desc, mip);

            const u32 packedRowPitch = mipWidth * bytesPerPixel;
            const u32 alignedRowPitch = AlignUp<u32>(packedRowPitch, TextureUtil::RowPitchAlignment);
            const u32 packedSlicePitch = packedRowPitch * mipHeight;
            const u32 alignedSlicePitch = alignedRowPitch * mipHeight;

            // Copy each depth slice (for 3D textures).
            for (u32 depthSlice = 0; depthSlice < mipDepth; ++depthSlice)
            {
                // Copy each row one-by-one with alignment.
                for (u32 row = 0; row < mipHeight; ++row)
                {
                    // From aligned to packed
                    const byte* srcRow = srcData + srcOffset + depthSlice * alignedSlicePitch + row * alignedRowPitch;
                    byte* dstRow = dstData + dstOffset + depthSlice * packedSlicePitch + row * packedRowPitch;

                    std::memcpy(dstRow, srcRow, packedRowPitch);
                }
            }

            // Move to next region in the packed source data.
            u64 regionStagingSize = static_cast<u64>(alignedSlicePitch) * mipDepth;
            srcOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);

            // Move to next aligned position in staging buffer.
            dstOffset += static_cast<u64>(packedSlicePitch) * mipDepth;
        }
    }
}

void WriteTextureDataAligned(const TextureDesc& desc,
                             Span<const TextureRegion> textureRegions,
                             Span<const byte> packedData,
                             Span<byte> alignedOutData)
{
    const u32 bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(desc.format);
    const byte* srcData = packedData.data();
    byte* dstData = alignedOutData.data();
    u64 srcOffset = 0;
    u64 dstOffset = 0;

    for (const TextureRegion& region : textureRegions)
    {
        for (u16 mip = region.subresource.startMip;
             mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
             ++mip)
        {
            const u32 mipWidth = region.extent.GetWidth(desc, mip);
            const u32 mipHeight = region.extent.GetHeight(desc, mip);
            const u32 mipDepth = region.extent.GetDepth(desc, mip);

            const u32 packedRowPitch = mipWidth * bytesPerPixel;
            const u32 alignedRowPitch = AlignUp<u32>(packedRowPitch, TextureUtil::RowPitchAlignment);
            const u32 packedSlicePitch = packedRowPitch * mipHeight;
            const u32 alignedSlicePitch = alignedRowPitch * mipHeight;

            // Copy each depth slice (for 3D textures).
            for (u32 depthSlice = 0; depthSlice < mipDepth; ++depthSlice)
            {
                // Copy each row one-by-one with alignment.
                for (u32 row = 0; row < mipHeight; ++row)
                {
                    const byte* srcRow = srcData + srcOffset + depthSlice * packedSlicePitch + row * packedRowPitch;
                    byte* dstRow = dstData + dstOffset + depthSlice * alignedSlicePitch + row * alignedRowPitch;

                    std::memcpy(dstRow, srcRow, packedRowPitch);
#if !VEX_SHIPPING
                    // Zero out padding bytes for debugging purposes.
                    if (alignedRowPitch > packedRowPitch)
                    {
                        std::memset(dstRow + packedRowPitch, 0, alignedRowPitch - packedRowPitch);
                    }
#endif
                }
            }

            // Move to next region in the packed source data.
            srcOffset += static_cast<u64>(packedSlicePitch) * mipDepth;

            // Move to next aligned position in staging buffer.
            u64 regionStagingSize = static_cast<u64>(alignedSlicePitch) * mipDepth;
            dstOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);
        }
    }
}

} // namespace TextureCopyUtil

std::vector<BufferTextureCopyDesc> BufferTextureCopyDesc::AllMips(const TextureDesc& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    TextureExtent3D mipSize{ desc.width, desc.height, desc.GetDepth() };

    std::vector<BufferTextureCopyDesc> bufferTextureCopyDescriptions;
    bufferTextureCopyDescriptions.reserve(desc.mips);

    u64 bufferOffset = 0;
    for (u16 mip = 0; mip < desc.mips; ++mip)
    {
        // Calculate aligned dimensions for this mip level
        const u32 packedRowSize = mipSize.width * texelByteSize;
        const u32 alignedRowPitch = AlignUp<u32>(packedRowSize, TextureUtil::RowPitchAlignment);
        const u32 alignedSlicePitch = alignedRowPitch * mipSize.height;

        u32 depthCount, sliceCount;
        if (desc.type == TextureType::Texture3D)
        {
            // For 3D textures: depth changes per mip, array count is always 1.
            depthCount = mipSize.depth;
            sliceCount = 1;
        }
        else
        {
            // For 2D array textures: depth is always 1, array count is constant.
            depthCount = 1;
            sliceCount = desc.depthOrSliceCount;
        }

        const u32 totalSlices = depthCount * sliceCount;
        const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * totalSlices;

        bufferTextureCopyDescriptions.push_back(BufferTextureCopyDesc{
            .bufferRegion = BufferRegion{ .offset = bufferOffset, .byteSize = alignedMipByteSize },
            .textureRegion = TextureRegion{
                .subresource = {
                    .startMip = mip,
                    .mipCount = 1,
                    .startSlice = 0,
                    .sliceCount = sliceCount,
                },
                .extent = { mipSize.width, mipSize.height, mipSize.depth },
            },
        });

        bufferOffset += alignedMipByteSize;
        bufferOffset = AlignUp<u64>(bufferOffset, TextureUtil::MipAlignment);
        mipSize = TextureExtent3D{
            std::max(1u, mipSize.width / 2u),
            std::max(1u, mipSize.height / 2u),
            std::max(1u, mipSize.depth / 2u),
        };
    }
    return bufferTextureCopyDescriptions;
}

std::vector<BufferTextureCopyDesc> BufferTextureCopyDesc::SingleMip(u16 mipIndex, const TextureDesc& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    std::vector<BufferTextureCopyDesc> bufferTextureCopyDescriptions(desc.GetSliceCount());

    TextureExtent3D mipSize{
        std::max(1u, desc.width >> mipIndex),
        std::max(1u, desc.height >> mipIndex),
        std::max(1u, desc.GetDepth() >> mipIndex),
    };

    // Calculate aligned dimensions for this mip level
    const u32 packedRowSize = mipSize.width * texelByteSize;
    const u32 alignedRowPitch = AlignUp<u32>(packedRowSize, TextureUtil::RowPitchAlignment);
    const u32 alignedSlicePitch = alignedRowPitch * mipSize.height;

    u32 depthCount, sliceCount;
    if (desc.type == TextureType::Texture3D)
    {
        // For 3D textures: depth changes per mip, array count is always 1.
        depthCount = mipSize.depth;
        sliceCount = 1;
    }
    else
    {
        // For 2D array textures: depth is always 1, array count is constant.
        depthCount = 1;
        sliceCount = desc.depthOrSliceCount;
    }

    const u32 totalSlices = depthCount * sliceCount;
    const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * totalSlices;

    bufferTextureCopyDescriptions.push_back(BufferTextureCopyDesc{
        .bufferRegion = BufferRegion{ .offset = 0, .byteSize = alignedMipByteSize },
        .textureRegion = TextureRegion{
            .subresource = {
                .startMip = mipIndex,
                .mipCount = 1,
                .startSlice = 0,
                .sliceCount = sliceCount,
            },
            .extent = { mipSize.width, mipSize.height, mipSize.depth },
        },
    });

    return bufferTextureCopyDescriptions;
}

} // namespace vex