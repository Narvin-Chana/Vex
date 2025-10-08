#include "ResourceCopy.h"

#include <cmath>
#include <cstring>

#include <Vex/ByteUtils.h>
#include <Vex/Validation.h>

namespace vex
{

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferTextureCopyDescription& copyDesc)
{
    BufferUtil::ValidateBufferSubresource(srcDesc, copyDesc.bufferSubresource);
    TextureUtil::ValidateTextureSubresource(dstDesc, copyDesc.textureSubresource);

    auto [mipWidth, mipHeight, mipDepth] = copyDesc.extent;
    const u32 requiredByteSize = static_cast<u32>(
        std::ceil(mipWidth * mipHeight * mipDepth * TextureUtil::GetPixelByteSizeFromFormat(dstDesc.format)));

    VEX_CHECK(copyDesc.bufferSubresource.size >= requiredByteSize,
              "Buffer not big enough to copy to texture. buffer size: {}, required mip byte size: {}",
              srcDesc.byteSize,
              requiredByteSize);
}

void ReadTextureDataAligned(const TextureDescription& textureDesc,
                            std::span<const TextureRegion> textureRegions,
                            std::span<const byte> alignedTextureData,
                            std::span<byte> packedOutputData)

{
    const u32 bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(textureDesc.format);
    const byte* srcData = alignedTextureData.data();
    byte* dstData = packedOutputData.data();
    u64 srcOffset = 0;
    u64 dstOffset = 0;

    for (const auto& region : textureRegions)
    {
        const u32 regionWidth = region.extent.width;
        const u32 regionHeight = region.extent.height;
        const u32 regionDepth = region.extent.depth;

        const u32 packedRowPitch = regionWidth * bytesPerPixel;
        const u32 alignedRowPitch = AlignUp<u32>(packedRowPitch, TextureUtil::RowPitchAlignment);
        const u32 packedSlicePitch = packedRowPitch * regionHeight;
        const u32 alignedSlicePitch = alignedRowPitch * regionHeight;

        // Copy each depth slice (for 3D textures).
        for (u32 depthSlice = 0; depthSlice < regionDepth; ++depthSlice)
        {
            // Copy each row one-by-one with alignment.
            for (u32 row = 0; row < regionHeight; ++row)
            {
                // From aligned to packed
                const byte* srcRow = srcData + srcOffset + depthSlice * alignedSlicePitch + row * alignedRowPitch;
                byte* dstRow = dstData + dstOffset + depthSlice * packedSlicePitch + row * packedRowPitch;

                std::memcpy(dstRow, srcRow, packedRowPitch);
            }
        }

        // Move to next region in the packed source data.
        u64 regionStagingSize = static_cast<u64>(alignedSlicePitch) * regionDepth;
        srcOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);

        // Move to next aligned position in staging buffer.
        dstOffset += static_cast<u64>(packedSlicePitch) * regionDepth;
    }
}

void WriteTextureDataAligned(const TextureDescription& textureDesc,
                             std::span<const TextureRegion> textureRegions,
                             std::span<const byte> packedData,
                             std::span<byte> alignedOutData)
{
    const u32 bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(textureDesc.format);
    const byte* srcData = packedData.data();
    byte* dstData = alignedOutData.data();
    u64 srcOffset = 0;
    u64 dstOffset = 0;

    for (const TextureRegion& region : textureRegions)
    {
        const u32 regionWidth = region.extent.width;
        const u32 regionHeight = region.extent.height;
        const u32 regionDepth = region.extent.depth;

        const u32 packedRowPitch = regionWidth * bytesPerPixel;
        const u32 alignedRowPitch = AlignUp<u32>(packedRowPitch, TextureUtil::RowPitchAlignment);
        const u32 packedSlicePitch = packedRowPitch * regionHeight;
        const u32 alignedSlicePitch = alignedRowPitch * regionHeight;

        // Copy each depth slice (for 3D textures).
        for (u32 depthSlice = 0; depthSlice < regionDepth; ++depthSlice)
        {
            // Copy each row one-by-one with alignment.
            for (u32 row = 0; row < regionHeight; ++row)
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
        srcOffset += static_cast<u64>(packedSlicePitch) * regionDepth;

        // Move to next aligned position in staging buffer.
        u64 regionStagingSize = static_cast<u64>(alignedSlicePitch) * regionDepth;
        dstOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);
    }
}

} // namespace TextureCopyUtil

std::vector<BufferTextureCopyDescription> BufferTextureCopyDescription::AllMips(const TextureDescription& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    TextureExtent mipSize{ desc.width, desc.height, desc.GetDepth() };

    std::vector<BufferTextureCopyDescription> bufferToTextureCopyDescriptions;
    bufferToTextureCopyDescriptions.reserve(desc.mips);

    u64 bufferOffset = 0;
    for (u16 mip = 0; mip < desc.mips; ++mip)
    {
        // Calculate aligned dimensions for this mip level
        const u32 packedRowSize = mipSize.width * texelByteSize;
        const u32 alignedRowPitch = AlignUp<u32>(packedRowSize, TextureUtil::RowPitchAlignment);
        const u32 alignedSlicePitch = alignedRowPitch * mipSize.height;

        u32 depthCount, arrayCount;
        if (desc.type == TextureType::Texture3D)
        {
            // For 3D textures: depth changes per mip, array count is always 1.
            depthCount = mipSize.depth;
            arrayCount = 1;
        }
        else
        {
            // For 2D array textures: depth is always 1, array count is constant.
            depthCount = 1;
            arrayCount = desc.depthOrArraySize;
        }

        const u32 totalSlices = depthCount * arrayCount;
        const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * totalSlices;

        bufferToTextureCopyDescriptions.push_back(BufferTextureCopyDescription{
            .bufferSubresource = BufferSubresource{ bufferOffset, alignedMipByteSize },
            .textureSubresource =
                TextureSubresource{
                    .mip = mip,
                    .startSlice = 0,
                    .sliceCount = arrayCount,
                    .offset = { 0, 0, 0 },
                },
            .extent = { mipSize.width, mipSize.height, mipSize.depth },
        });

        bufferOffset += alignedMipByteSize;
        bufferOffset = AlignUp<u64>(bufferOffset, TextureUtil::MipAlignment);
        mipSize = TextureExtent{
            std::max(1u, mipSize.width / 2u),
            std::max(1u, mipSize.height / 2u),
            std::max(1u, mipSize.depth / 2u),
        };
    }
    return bufferToTextureCopyDescriptions;
}

std::vector<BufferTextureCopyDescription> BufferTextureCopyDescription::AllMip(u16 mipIndex,
                                                                               const TextureDescription& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    std::vector<BufferTextureCopyDescription> bufferToTextureCopyDescriptions(desc.GetArraySize());

    TextureExtent mipSize = TextureExtent{
        std::max(1u, desc.width >> mipIndex),
        std::max(1u, desc.height >> mipIndex),
        std::max(1u, desc.GetDepth() >> mipIndex),
    };

    // Calculate aligned dimensions for this mip level
    const u32 packedRowSize = mipSize.width * texelByteSize;
    const u32 alignedRowPitch = AlignUp<u32>(packedRowSize, TextureUtil::RowPitchAlignment);
    const u32 alignedSlicePitch = alignedRowPitch * mipSize.height;

    u32 depthCount, arrayCount;
    if (desc.type == TextureType::Texture3D)
    {
        // For 3D textures: depth changes per mip, array count is always 1.
        depthCount = mipSize.depth;
        arrayCount = 1;
    }
    else
    {
        // For 2D array textures: depth is always 1, array count is constant.
        depthCount = 1;
        arrayCount = desc.depthOrArraySize;
    }

    const u32 totalSlices = depthCount * arrayCount;
    const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * totalSlices;

    bufferToTextureCopyDescriptions.push_back(BufferTextureCopyDescription{
        .bufferSubresource = BufferSubresource{ 0, alignedMipByteSize },
        .textureSubresource =
            TextureSubresource{
                .mip = mipIndex,
                .startSlice = 0,
                .sliceCount = arrayCount,
                .offset = { 0, 0, 0 },
            },
        .extent = { mipSize.width, mipSize.height, mipSize.depth },
    });

    return bufferToTextureCopyDescriptions;
}

} // namespace vex