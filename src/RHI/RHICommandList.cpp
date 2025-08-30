#include "RHICommandList.h"

#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex
{

void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    const auto desc = src.GetDescription();
    std::vector<std::pair<TextureRegion, TextureRegionExtent>> regions;
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.depthOrArraySize;
    for (u32 i = 0; i < desc.mips; ++i)
    {
        regions.emplace_back(
            TextureRegion{
                .mip = i,
                .baseLayer = 0,
                .layerCount = desc.type == TextureType::Texture3D ? desc.depthOrArraySize : 1,
                .offset = { 0, 0, 0 }
            },
            TextureRegionExtent{ width, height, depth }
        );

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }

    std::vector<TextureToTextureCopyRegionMapping> regionMappings;
    for (const auto& [region, extent] : regions)
    {
        regionMappings.emplace_back(region, region, extent);
    }

    Copy(src, dst, regionMappings);
}

void RHICommandListBase::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    Copy(src, dst, BufferToBufferCopyRegion{ 0, 0, src.GetDescription().byteSize });
}

void RHICommandListBase::Copy(RHIBuffer& src, RHITexture& dst)
{
    const TextureDescription& desc = dst.GetDescription();

    const u8 texelByteSize = GetPixelByteSizeFromFormat(desc.format);

    TextureRegionExtent mipSize{ desc.width,
                                desc.height,
                                (desc.type == TextureType::Texture3D) ? desc.depthOrArraySize : 1 };

    std::vector<BufferToTextureCopyMapping> regions;

    u32 bufferOffset = 0;
    for (u8 i = 0; i < desc.mips; ++i)
    {
        const u32 mipByteSize = mipSize.width * mipSize.height * (desc.type == TextureType::Texture3D
                                    ? mipSize.depth
                                    : desc.depthOrArraySize * texelByteSize);

        regions.push_back(BufferToTextureCopyMapping{
            .srcRegion = BufferRegion{ bufferOffset, mipByteSize },
            .dstRegion = TextureRegion{
                .mip = i,
                .baseLayer = 0,
                .layerCount = (desc.type == TextureType::Texture3D) ? 1 : desc.depthOrArraySize,
                .offset = { 0, 0, 0 }
            },
            .extent = { mipSize.width, mipSize.height, mipSize.depth }
        });

        bufferOffset += mipByteSize;
        mipSize = TextureRegionExtent{
            std::max(1u, mipSize.width / 2u),
            std::max(1u, mipSize.height / 2u),
            std::max(1u, mipSize.depth / 2u)
        };
    }

    Copy(src, dst, regions);
}

}