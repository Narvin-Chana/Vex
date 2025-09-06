#include "RHICommandList.h"

#include <Vex/ByteUtils.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Validation.h>

namespace vex
{

void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    const TextureDescription& desc = src.GetDescription();
    std::vector<TextureCopyDescription> copyDescriptions;
    copyDescriptions.reserve(desc.mips);
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.depthOrArraySize;
    for (u16 mip = 0; mip < desc.mips; ++mip)
    {
        TextureSubresource subresource{ .mip = mip,
                                        .startSlice = 0,
                                        .sliceCount = desc.GetArrayCount(),
                                        .offset = { 0, 0, 0 } };
        copyDescriptions.emplace_back(subresource, subresource, TextureExtent{ width, height, depth });

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }

    Copy(src, dst, copyDescriptions);
}

void RHICommandListBase::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    Copy(src, dst, BufferCopyDescription{ 0, 0, src.GetDescription().byteSize });
}

void RHICommandListBase::Copy(RHIBuffer& src, RHITexture& dst)
{
    const TextureDescription& desc = dst.GetDescription();

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

        // Calculate total aligned size for this mip level
        const u32 depthOrArrayCount = (desc.type == TextureType::Texture3D) ? mipSize.depth : desc.depthOrArraySize;
        const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * depthOrArrayCount;

        bufferToTextureCopyDescriptions.push_back(
            BufferToTextureCopyDescription{ .srcSubresource = BufferSubresource{ bufferOffset, alignedMipByteSize },
                                            .dstSubresource = TextureSubresource{ .mip = mip,
                                                                                  .startSlice = 0,
                                                                                  .sliceCount = desc.GetArrayCount(),
                                                                                  .offset = { 0, 0, 0 } },
                                            .extent = { mipSize.width, mipSize.height, mipSize.depth } });

        bufferOffset += alignedMipByteSize;
        bufferOffset = AlignUp<u64>(bufferOffset, TextureUtil::MipAlignment);
        mipSize = TextureExtent{ std::max(1u, mipSize.width / 2u),
                                 std::max(1u, mipSize.height / 2u),
                                 std::max(1u, mipSize.depth / 2u) };
    }

    Copy(src, dst, bufferToTextureCopyDescriptions);
}

} // namespace vex