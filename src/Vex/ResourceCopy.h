#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{

struct BufferToTextureCopyDescription
{
    BufferSubresource srcSubresource{};
    TextureSubresource dstSubresource{};
    TextureExtent extent{};
};

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferToTextureCopyDescription& copyDesc);
} // namespace TextureCopyUtil

} // namespace vex