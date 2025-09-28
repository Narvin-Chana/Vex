#include "RHICommandList.h"

#include <Vex/ByteUtils.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Validation.h>

namespace vex
{

void RHICommandListBase::BufferBarrier(RHIBuffer& buffer, RHIBarrierSync sync, RHIBarrierAccess access)
{
    RHIBufferBarrier barrier{ buffer, sync, access };
    Barrier({ &barrier, 1 }, {});
}

void RHICommandListBase::TextureBarrier(RHITexture& texture,
                                        RHIBarrierSync sync,
                                        RHIBarrierAccess access,
                                        RHITextureLayout layout)
{
    RHITextureBarrier barrier{ texture, sync, access, layout };
    Barrier({}, { &barrier, 1 });
}

void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    TextureCopyDesc copyDesc;
    Copy(src, dst, std::span(&copyDesc, 1));
}

void RHICommandListBase::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    Copy(src, dst, BufferCopyDesc{ 0, 0, src.GetDesc().byteSize });
}

void RHICommandListBase::Copy(RHIBuffer& src, RHITexture& dst)
{
    const TextureDesc& desc = dst.GetDesc();

    const float texelByteSize = TextureUtil::GetPixelByteSizeFromFormat(desc.format);

    TextureExtent3D mipSize{ desc.width, desc.height, desc.GetDepth() };

    std::vector<BufferToTextureCopyDesc> bufferToTextureCopyDescriptions;
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
            arrayCount = desc.depthOrSliceCount;
        }

        const u32 totalSlices = depthCount * arrayCount;
        const u64 alignedMipByteSize = static_cast<u64>(alignedSlicePitch) * totalSlices;

        bufferToTextureCopyDescriptions.push_back(BufferToTextureCopyDesc{
            .srcRegion = BufferRegion{ bufferOffset, alignedMipByteSize },
            .dstRegion = TextureRegion{ .subresource = TextureSubresource{ .startMip = mip,
                                                                           .mipCount = 1,
                                                                           .startSlice = 0,
                                                                           .sliceCount = arrayCount } },
        });

        TextureCopyUtil::ValidateBufferToTextureCopyDesc(src.GetDesc(),
                                                         dst.GetDesc(),
                                                         bufferToTextureCopyDescriptions.back());

        bufferOffset += alignedMipByteSize;
        bufferOffset = AlignUp<u64>(bufferOffset, TextureUtil::MipAlignment);
        mipSize = TextureExtent3D{
            std::max(1u, mipSize.width / 2u),
            std::max(1u, mipSize.height / 2u),
            std::max(1u, mipSize.depth / 2u),
        };
    }

    Copy(src, dst, bufferToTextureCopyDescriptions);
}

} // namespace vex