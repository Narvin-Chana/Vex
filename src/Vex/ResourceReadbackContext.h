#pragma once

#include <memory>

#include <Vex/Buffer.h>
#include <Vex/Containers/Span.h>
#include <Vex/Synchronization.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/NonNullPtr.h>

namespace vex
{

class Graphics;

// Invalid until Graphics::Submit sets it to the token of the submission containing the readback.
using ReadbackSubmissionToken = std::shared_ptr<SyncToken>;

// ReadData waits for the readback's submission to be done on the GPU.
class BufferReadbackContext
{
public:
    ~BufferReadbackContext();
    BufferReadbackContext(BufferReadbackContext&& other);
    BufferReadbackContext& operator=(BufferReadbackContext&& other);

    void ReadData(Span<byte> outData) const;
    [[nodiscard]] u64 GetDataByteSize() const;

private:
    BufferReadbackContext(Buffer buffer, Graphics& graphics, ReadbackSubmissionToken submissionToken);

    Buffer buffer;
    NonNullPtr<Graphics> graphics;
    ReadbackSubmissionToken submissionToken;
    friend class CommandContext;
};

// ReadData waits for the readback's submission to be done on the GPU.
class TextureReadbackContext
{
public:
    ~TextureReadbackContext();
    TextureReadbackContext(TextureReadbackContext&& other);
    TextureReadbackContext& operator=(TextureReadbackContext&& other);

    void ReadData(Span<byte> outData) const;
    [[nodiscard]] u64 GetDataByteSize() const;
    [[nodiscard]] TextureDesc GetSourceTextureDescription() const
    {
        return textureDesc;
    };
    [[nodiscard]] std::vector<TextureRegion> GetReadbackRegions() const
    {
        return textureRegions;
    }

private:
    TextureReadbackContext(Buffer buffer,
                           Span<const TextureRegion> textureRegions,
                           TextureDesc textureDesc,
                           Graphics& graphics,
                           ReadbackSubmissionToken submissionToken);

    // Buffer contains readback data from the GPU.
    // This data is aligned according to Vex internal alignment
    Buffer buffer;
    std::vector<TextureRegion> textureRegions;
    TextureDesc textureDesc;

    NonNullPtr<Graphics> graphics;
    ReadbackSubmissionToken submissionToken;
    friend class CommandContext;
};
} // namespace vex
