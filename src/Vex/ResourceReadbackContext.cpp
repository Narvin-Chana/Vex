#include "ResourceReadbackContext.h"

#include <Vex/ResourceCopy.h>

namespace vex
{

TextureReadbackContext::TextureReadbackContext(const Buffer& buffer,
                                               std::span<const TextureRegion> textureRegions,
                                               const TextureDesc& desc,
                                               Graphics& backend)
    : buffer{ buffer }
    , textureRegions{ textureRegions.begin(), textureRegions.end() }
    , textureDesc{ desc }
    , backend{ backend }
{
}

void BufferReadbackContext::ReadData(std::span<byte> outData)
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);

    std::span<byte> bufferData = rhiBuffer.Map();
    std::copy(bufferData.begin(), bufferData.begin() + outData.size(), outData.begin());
    rhiBuffer.Unmap();
}

u64 BufferReadbackContext::GetDataByteSize() const noexcept
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

void TextureReadbackContext::ReadData(std::span<byte> outData) const
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);

    std::span<byte> bufferData = rhiBuffer.Map();
    TextureCopyUtil::ReadTextureDataAligned(textureDesc, textureRegions, bufferData, outData);
    rhiBuffer.Unmap();
}

u64 TextureReadbackContext::GetDataByteSize() const noexcept
{
    return TextureUtil::ComputePackedTextureDataByteSize(textureDesc, textureRegions);
}
} // namespace vex