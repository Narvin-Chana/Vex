#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{

struct BufferTextureCopyDescription
{
    BufferSubresource bufferSubresource{};
    TextureSubresource textureSubresource{};
    TextureExtent extent{};

    // Uploads the entirety of the texture (all mips and all depth slices).
    static std::vector<BufferTextureCopyDescription> CopyAllMips(const TextureDescription& textureDescription);
    // Uploads the entirety of a specific mip of the texture (1 mip and all depth slices).
    static std::vector<BufferTextureCopyDescription> CopyMip(u16 mipIndex,
                                                             const TextureDescription& textureDescription);
};

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferTextureCopyDescription& copyDesc);
} // namespace TextureCopyUtil

} // namespace vex