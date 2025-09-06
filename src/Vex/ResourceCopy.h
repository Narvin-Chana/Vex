#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{

struct BufferToTextureCopyDescription
{
    BufferSubresource srcRegion{};
    TextureSubresource dstRegion{};
    TextureExtent extent{};
};

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferToTextureCopyDescription& copyDesc);
void ValidateSimpleBufferToTextureCopy(const BufferDescription& srcDesc, const TextureDescription& dstDesc);
} // namespace TextureCopyUtil

} // namespace vex