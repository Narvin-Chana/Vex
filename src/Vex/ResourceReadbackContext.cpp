#include "ResourceReadbackContext.h"

#include <Vex/Graphics.h>
#include <Vex/ResourceCopy.h>

namespace vex
{

TextureReadbackContext::TextureReadbackContext(const Buffer& buffer,
                                               Span<const TextureRegion> textureRegions,
                                               const TextureDesc& desc,
                                               Graphics& graphics)
    : buffer{ buffer }
    , textureRegions{ textureRegions.begin(), textureRegions.end() }
    , textureDesc{ desc }
    , graphics{ graphics }
{
}

void BufferReadbackContext::ReadData(Span<byte> outData)
{
    RHIBuffer& rhiBuffer = graphics->GetRHIBuffer(buffer.handle);
    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    std::copy(bufferData.begin(), bufferData.begin() + outData.size(), outData.begin());
}

u64 BufferReadbackContext::GetDataByteSize() const
{
    return buffer.desc.byteSize;
}

BufferReadbackContext::~BufferReadbackContext()
{
    graphics->DestroyBuffer(buffer);
}

BufferReadbackContext::BufferReadbackContext(BufferReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , graphics{ other.graphics }
{
}

BufferReadbackContext& BufferReadbackContext::operator=(BufferReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        graphics = other.graphics;
    }
    return *this;
}

BufferReadbackContext::BufferReadbackContext(const Buffer& buffer, Graphics& graphics)
    : buffer{ buffer }
    , graphics{ graphics }
{
}

TextureReadbackContext::~TextureReadbackContext()
{
    if (buffer.handle != GInvalidBufferHandle)
    {
        graphics->DestroyBuffer(buffer);
    }
}

TextureReadbackContext::TextureReadbackContext(TextureReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , textureRegions{ std::move(other.textureRegions) }
    , textureDesc{ std::move(other.textureDesc) }
    , graphics{ other.graphics }
{
}

TextureReadbackContext& TextureReadbackContext::operator=(TextureReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        std::swap(textureRegions, other.textureRegions);
        std::swap(textureDesc, other.textureDesc);
        graphics = other.graphics;
    }
    return *this;
}

void TextureReadbackContext::ReadData(Span<byte> outData) const
{
    RHIBuffer& rhiBuffer = graphics->GetRHIBuffer(buffer.handle);

    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    TextureCopyUtil::ReadTextureDataAligned(textureDesc, textureRegions, bufferData, outData);
}

u64 TextureReadbackContext::GetDataByteSize() const
{
    return TextureUtil::ComputePackedTextureDataByteSize(textureDesc, textureRegions);
}
} // namespace vex