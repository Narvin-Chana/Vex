#include "RHICommandList.h"

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Validation.h>

namespace vex
{

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferToTextureCopyDescription& copyDesc)
{
    BufferUtil::ValidateBufferSubresource(srcDesc, copyDesc.srcRegion);
    TextureUtil::ValidateTextureSubresource(dstDesc, copyDesc.dstRegion);

    auto [mipWidth, mipHeight, mipDepth] = copyDesc.extent;
    const u32 requiredByteSize = static_cast<u32>(
        std::ceil(mipWidth * mipHeight * mipDepth * TextureUtil::GetPixelByteSizeFromFormat(dstDesc.format)));

    if (copyDesc.srcRegion.size < requiredByteSize)
    {
        LogFailValidation("Buffer not big enough to copy to texture. buffer size: {}, required mip byte size: {}",
                          srcDesc.byteSize,
                          requiredByteSize);
    }
}

void ValidateSimpleBufferToTextureCopy(const BufferDescription& srcDesc, const TextureDescription& dstDesc)
{
    u32 textureByteSize = TextureUtil::GetTotalTextureByteSize(dstDesc);
    if (srcDesc.byteSize < textureByteSize)
    {
        LogFailValidation("Buffer not big enough to copy to texture. buffer size: {}, texture byte size: {}",
                          srcDesc.byteSize,
                          textureByteSize);
    }
}
} // namespace TextureCopyUtil

void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    const auto desc = src.GetDescription();
    std::vector<std::pair<TextureSubresource, TextureExtent>> regions;
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.depthOrArraySize;
    for (u32 i = 0; i < desc.mips; ++i)
    {
        regions.emplace_back(
            TextureSubresource{ .mip = i, .startSlice = 0, .sliceCount = desc.GetArrayCount(), .offset = { 0, 0, 0 } },
            TextureExtent{ width, height, depth });

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }

    std::vector<TextureCopyDescription> regionMappings;
    for (const auto& [region, extent] : regions)
    {
        regionMappings.emplace_back(region, region, extent);
    }

    Copy(src, dst, regionMappings);
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

    std::vector<BufferToTextureCopyDescription> regions;

    u32 bufferOffset = 0;
    for (u16 i = 0; i < desc.mips; ++i)
    {
        const u32 mipByteSize = static_cast<u32>(
            std::ceil(mipSize.width * mipSize.height *
                      (desc.type == TextureType::Texture3D ? mipSize.depth : desc.depthOrArraySize) * texelByteSize));

        regions.push_back(
            BufferToTextureCopyDescription{ .srcRegion = BufferSubresource{ bufferOffset, mipByteSize },
                                            .dstRegion = TextureSubresource{ .mip = i,
                                                                             .startSlice = 0,
                                                                             .sliceCount = desc.GetArrayCount(),
                                                                             .offset = { 0, 0, 0 } },
                                            .extent = { mipSize.width, mipSize.height, mipSize.depth } });

        bufferOffset += mipByteSize;
        mipSize = TextureExtent{ std::max(1u, mipSize.width / 2u),
                                 std::max(1u, mipSize.height / 2u),
                                 std::max(1u, mipSize.depth / 2u) };
    }

    Copy(src, dst, regions);
}

} // namespace vex