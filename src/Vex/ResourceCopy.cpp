#include "ResourceCopy.h"

#include <cmath>

#include <Vex/Validation.h>

namespace vex
{

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDesc(const BufferDesc& srcDesc,
                                     const TextureDesc& dstDesc,
                                     const BufferToTextureCopyDesc& copyDesc)
{
    BufferUtil::ValidateBufferRegion(srcDesc, copyDesc.srcRegion);
    copyDesc.dstRegion.Validate(dstDesc);

    auto [width, height, depth] = copyDesc.dstRegion.extent;
    const u32 requiredPackedByteSize =
        static_cast<u32>(std::ceil(width * height * depth * TextureUtil::GetPixelByteSizeFromFormat(dstDesc.format)));

    VEX_CHECK(copyDesc.srcRegion.size >= requiredPackedByteSize,
              "Buffer not big enough to copy to texture. buffer size: {}, required mip byte size: {}",
              srcDesc.byteSize,
              requiredPackedByteSize);
}
} // namespace TextureCopyUtil

BufferToTextureCopyDesc BufferToTextureCopyDesc::Resolve(const BufferDesc& srcDesc, const TextureDesc& dstDesc) const
{
    return BufferToTextureCopyDesc{
        .srcRegion = srcRegion.Resolve(srcDesc),
        .dstRegion = dstRegion.Resolve(dstDesc),
    };
}

} // namespace vex