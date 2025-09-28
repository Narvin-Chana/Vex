#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{

struct BufferToTextureCopyDesc
{
    BufferRegion srcRegion;
    TextureRegion dstRegion;

    constexpr bool operator==(const BufferToTextureCopyDesc&) const = default;
};

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDesc(const BufferDesc& srcDesc,
                                     const TextureDesc& dstDesc,
                                     const BufferToTextureCopyDesc& copyDesc);
} // namespace TextureCopyUtil

} // namespace vex