#include "ResourceCopy.h"

#include <cmath>

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

} // namespace vex