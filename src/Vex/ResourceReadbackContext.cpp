#include "ResourceReadbackContext.h"

#include <Vex/ResourceCopy.h>

namespace vex
{

TextureReadbackContext::TextureReadbackContext(const Buffer& buffer,
                                               Span<const TextureRegion> textureRegions,
                                               const TextureDesc& desc,
                                               Graphics& backend)
    : buffer{ buffer }
    , textureRegions{ textureRegions.begin(), textureRegions.end() }
    , textureDesc{ desc }
    , backend{ backend }
{
}

void BufferReadbackContext::ReadData(Span<byte> outData)
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);
    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    std::copy(bufferData.begin(), bufferData.begin() + outData.size(), outData.begin());
}

u64 BufferReadbackContext::GetDataByteSize() const
{
    return buffer.desc.byteSize;
}

BufferReadbackContext::~BufferReadbackContext()
{
    backend->DestroyBuffer(buffer);
}

BufferReadbackContext::BufferReadbackContext(BufferReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , backend{ other.backend }
{
}

BufferReadbackContext& BufferReadbackContext::operator=(BufferReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        backend = other.backend;
    }
    return *this;
}

BufferReadbackContext::BufferReadbackContext(const Buffer& buffer, Graphics& backend)
    : buffer{ buffer }
    , backend{ backend }
{
}

TextureReadbackContext::~TextureReadbackContext()
{
    if (buffer.handle != GInvalidBufferHandle)
    {
        backend->DestroyBuffer(buffer);
    }
}

TextureReadbackContext::TextureReadbackContext(TextureReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , textureRegions{ std::move(other.textureRegions) }
    , textureDesc{ std::move(other.textureDesc) }
    , backend{ other.backend }
{
}

TextureReadbackContext& TextureReadbackContext::operator=(TextureReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        std::swap(textureRegions, other.textureRegions);
        std::swap(textureDesc, other.textureDesc);
        backend = other.backend;
    }
    return *this;
}

void TextureReadbackContext::ReadData(Span<byte> outData) const
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);

    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    TextureCopyUtil::ReadTextureDataAligned(textureDesc, textureRegions, bufferData, outData);
}

u64 TextureReadbackContext::GetDataByteSize() const
{
    return TextureUtil::ComputePackedTextureDataByteSize(textureDesc, textureRegions);
}
} // namespace vex