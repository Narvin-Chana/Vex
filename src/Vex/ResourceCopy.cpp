#include "ResourceCopy.h"

#include <cmath>

#include <Vex/ByteUtils.h>
#include <Vex/Validation.h>

namespace vex
{

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferToTextureCopyDescription& copyDesc)
{
    BufferUtil::ValidateBufferSubresource(srcDesc, copyDesc.srcSubresource);
    TextureUtil::ValidateTextureSubresource(dstDesc, copyDesc.dstSubresource);

    auto [mipWidth, mipHeight, mipDepth] = copyDesc.extent;
    const u32 requiredByteSize = static_cast<u32>(
        std::ceil(mipWidth * mipHeight * mipDepth * TextureUtil::GetPixelByteSizeFromFormat(dstDesc.format)));

    VEX_CHECK(copyDesc.srcSubresource.size >= requiredByteSize,
              "Buffer not big enough to copy to texture. buffer size: {}, required mip byte size: {}",
              srcDesc.byteSize,
              requiredByteSize);
}
} // namespace TextureCopyUtil

std::vector<BufferToTextureCopyDescription> BufferToTextureCopyDescription::CopyAllMips(const TextureDescription& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    TextureExtent mipSize{ desc.width, desc.height, desc.GetDepth() };

    std::vector<BufferToTextureCopyDescription> bufferToTextureCopyDescriptions;
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

        bufferToTextureCopyDescriptions.push_back(BufferToTextureCopyDescription{
            .srcSubresource = BufferSubresource{ bufferOffset, alignedMipByteSize },
            .dstSubresource =
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

std::vector<BufferToTextureCopyDescription> BufferToTextureCopyDescription::CopyMip(u16 mipIndex,
                                                                                    const TextureDescription& desc)
{
    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    std::vector<BufferToTextureCopyDescription> bufferToTextureCopyDescriptions(desc.GetArraySize());

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

    bufferToTextureCopyDescriptions.push_back(BufferToTextureCopyDescription{
        .srcSubresource = BufferSubresource{ 0, alignedMipByteSize },
        .dstSubresource =
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