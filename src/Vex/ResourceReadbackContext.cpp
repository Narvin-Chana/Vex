#include "ResourceReadbackContext.h"

#include <utility>

#include <Vex/Graphics.h>
#include <Vex/ResourceCopy.h>
#include <VexMacros.h>

namespace vex
{

namespace ResourceReadbackContext_Internal
{

static void WaitForReadbackSubmission(Graphics& graphics, const ReadbackSubmissionToken& submissionToken)
{
    VEX_CHECK(submissionToken && *submissionToken != GInvalidSyncToken,
              "Cannot read back data before the command context containing the readback was submitted.");
    graphics.WaitForTokenOnCPU(*submissionToken);
}

} // namespace ResourceReadbackContext_Internal

BufferReadbackContext::~BufferReadbackContext()
{
    graphics->DestroyBuffer(buffer);
}

BufferReadbackContext::BufferReadbackContext(BufferReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , graphics{ other.graphics }
    , submissionToken{ std::move(other.submissionToken) }
{
}

BufferReadbackContext& BufferReadbackContext::operator=(BufferReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        std::swap(submissionToken, other.submissionToken);
        graphics = other.graphics;
    }
    return *this;
}

void BufferReadbackContext::ReadData(Span<byte> outData) const
{
    ResourceReadbackContext_Internal::WaitForReadbackSubmission(*graphics, submissionToken);

    const RHIBuffer& rhiBuffer = graphics->GetRHIBuffer(buffer.handle);
    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    std::copy_n(bufferData.begin(), outData.size(), outData.begin());
}

u64 BufferReadbackContext::GetDataByteSize() const
{
    return buffer.desc.byteSize;
}

BufferReadbackContext::BufferReadbackContext(Buffer buffer,
                                             Graphics& graphics,
                                             ReadbackSubmissionToken submissionToken)
    : buffer{ std::move(buffer) }
    , graphics{ graphics }
    , submissionToken{ std::move(submissionToken) }
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
    , submissionToken{ std::move(other.submissionToken) }
{
}

TextureReadbackContext& TextureReadbackContext::operator=(TextureReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        std::swap(textureRegions, other.textureRegions);
        std::swap(textureDesc, other.textureDesc);
        std::swap(submissionToken, other.submissionToken);
        graphics = other.graphics;
    }
    return *this;
}

void TextureReadbackContext::ReadData(Span<byte> outData) const
{
    ResourceReadbackContext_Internal::WaitForReadbackSubmission(*graphics, submissionToken);

    const RHIBuffer& rhiBuffer = graphics->GetRHIBuffer(buffer.handle);
    Span<const byte> bufferData = rhiBuffer.GetMappedData();
    TextureCopyUtil::ReadTextureDataAligned(textureDesc, textureRegions, bufferData, outData);
}

u64 TextureReadbackContext::GetDataByteSize() const
{
    return TextureUtil::ComputePackedTextureDataByteSize(textureDesc, textureRegions);
}

TextureReadbackContext::TextureReadbackContext(Buffer buffer,
                                               Span<const TextureRegion> textureRegions,
                                               TextureDesc textureDesc,
                                               Graphics& graphics,
                                               ReadbackSubmissionToken submissionToken)
    : buffer{ std::move(buffer) }
    , textureRegions{ textureRegions.begin(), textureRegions.end() }
    , textureDesc{ std::move(textureDesc) }
    , graphics{ graphics }
    , submissionToken{ std::move(submissionToken) }
{
}
} // namespace vex
