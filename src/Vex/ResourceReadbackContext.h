#pragma once

#include <Vex/Buffer.h>
#include <Vex/Containers/Span.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/NonNullPtr.h>

namespace vex
{

class Graphics;

class BufferReadbackContext
{
public:
    ~BufferReadbackContext();
    BufferReadbackContext(BufferReadbackContext&& other);
    BufferReadbackContext& operator=(BufferReadbackContext&& other);

    void ReadData(Span<byte> outData);
    [[nodiscard]] u64 GetDataByteSize() const;

private:
    BufferReadbackContext(const Buffer& buffer, Graphics& graphics);

    Buffer buffer;
    NonNullPtr<Graphics> graphics;
    friend class CommandContext;
};

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
    TextureReadbackContext(const Buffer& buffer,
                           Span<const TextureRegion> textureRegions,
                           const TextureDesc& textureDesc,
                           Graphics& graphics);

    // Buffer contains readback data from the GPU.
    // This data is aligned according to Vex internal alignment
    Buffer buffer;
    std::vector<TextureRegion> textureRegions;
    TextureDesc textureDesc;

    NonNullPtr<Graphics> graphics;
    friend class CommandContext;
};
} // namespace vex
