#pragma once

#include <Vex/Buffer.h>
#include <Vex/Texture.h>

namespace vex
{
class TextureReadbackContext;
}
namespace vex
{
class Graphics;

struct BufferTextureCopyDesc
{
    BufferRegion bufferRegion;
    TextureRegion textureRegion;
    constexpr bool operator==(const BufferTextureCopyDesc&) const = default;

    static std::vector<BufferTextureCopyDesc> AllMips(const TextureDesc& desc);
    static std::vector<BufferTextureCopyDesc> SingleMip(u16 mipIndex, const TextureDesc& desc);
};

namespace TextureCopyUtil
{

void ValidateBufferTextureCopyDesc(const BufferDesc& srcDesc,
                                   const TextureDesc& dstDesc,
                                   const BufferTextureCopyDesc& copyDesc);

// Reads data according to alignment. The texture regions provided need describe how the data is laid out in memory
// The textureDesc and texture regions represent the layout of the aligned data.
// If the desc and regions are wrong the data will be badly interpreted
void ReadTextureDataAligned(const TextureDesc& textureDesc,
                            std::span<const TextureRegion> textureRegions,
                            std::span<const byte> alignedTextureData,
                            std::span<byte> packedOutputData);

// This aligns packed data into another buffer matching the textureRegions and textureDesc provided.
void WriteTextureDataAligned(const TextureDesc& textureDesc,
                             std::span<const TextureRegion> textureRegions,
                             std::span<const byte> packedData,
                             std::span<byte> alignedOutData);

} // namespace TextureCopyUtil

} // namespace vex