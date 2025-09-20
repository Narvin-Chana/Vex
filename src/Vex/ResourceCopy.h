#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{
class TextureReadbackContext;
}
namespace vex
{
class GfxBackend;

struct BufferTextureCopyDescription
{
    BufferSubresource bufferSubresource{};
    TextureSubresource textureSubresource{};
    TextureExtent extent{};

    // Copies the entirety of the texture (all mips and all depth slices).
    static std::vector<BufferTextureCopyDescription> AllMips(const TextureDescription& textureDescription);
    // Copies the entirety of a specific mip of the texture (1 mip and all depth slices).
    static std::vector<BufferTextureCopyDescription> AllMip(u16 mipIndex, const TextureDescription& textureDescription);
};

namespace TextureCopyUtil
{
void ValidateBufferToTextureCopyDescription(const BufferDescription& srcDesc,
                                            const TextureDescription& dstDesc,
                                            const BufferTextureCopyDescription& copyDesc);

// Reads data according to alignment. The texture regions provided need describe how the data is laid out in memory
// The textureDesc and texture regions represent the layout of the aligned data.
// If the desc and regions are wrong the data will be badly interpreted
void ReadTextureDataAligned(const TextureDescription& textureDesc,
                            std::span<const TextureRegion> textureRegions,
                            std::span<const byte> alignedTextureData,
                            std::span<byte> packedOutputData);

// This aligns packed data into another buffer matching the textureRegions and textureDesc provided.
void WriteTextureDataAligned(const TextureDescription& textureDesc,
                             std::span<const TextureRegion> textureRegions,
                             std::span<const byte> packedData,
                             std::span<byte> alignedOutData);

} // namespace TextureCopyUtil

} // namespace vex