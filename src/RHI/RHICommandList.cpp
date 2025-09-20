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
                                        .sliceCount = desc.GetArraySize(),
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
    std::vector<BufferTextureCopyDescription> bufferToTextureCopies =
        BufferTextureCopyDescription::AllMips(dst.GetDescription());

    for (const auto& copy : bufferToTextureCopies)
    {
        TextureCopyUtil::ValidateBufferToTextureCopyDescription(src.GetDescription(), dst.GetDescription(), copy);
    }

    Copy(src, dst, bufferToTextureCopies);
}

void RHICommandListBase::Copy(RHITexture& src, RHIBuffer& dst)
{
    std::vector<BufferTextureCopyDescription> bufferToTextureCopies =
        BufferTextureCopyDescription::AllMips(src.GetDescription());

    for (const auto& copy : bufferToTextureCopies)
    {
        TextureCopyUtil::ValidateBufferToTextureCopyDescription(dst.GetDescription(), src.GetDescription(), copy);
    }

    Copy(src, dst, bufferToTextureCopies);
}

} // namespace vex