#pragma once
#include <span>

#include <Vex/Graphics.h>
#include <Vex/Types.h>

namespace vex
{
class BufferReadbackContext
{
public:
    ~BufferReadbackContext();
    BufferReadbackContext(BufferReadbackContext&& other);
    BufferReadbackContext& operator=(BufferReadbackContext&& other);

    void ReadData(std::span<vex::byte> outData);
    [[nodiscard]] u64 GetDataByteSize() const noexcept;

private:
    BufferReadbackContext(const Buffer& buffer, vex::Graphics& backend);

    Buffer buffer;
    NonNullPtr<Graphics> backend;
    friend class CommandContext;
};

class TextureReadbackContext
{
public:
    ~TextureReadbackContext();
    TextureReadbackContext(TextureReadbackContext&& other);
    TextureReadbackContext& operator=(TextureReadbackContext&& other);

    void ReadData(std::span<byte> outData);
    [[nodiscard]] u64 GetDataByteSize() const noexcept;
    [[nodiscard]] TextureDesc GetSourceTextureDescription() const noexcept
    {
        return textureDesc;
    };
    [[nodiscard]] std::vector<TextureRegion> GetReadbackRegions() const noexcept
    {
        return textureRegions;
    }

private:
    TextureReadbackContext(const Buffer& buffer,
                           std::span<const TextureRegion> textureRegions,
                           const TextureDesc& textureDesc,
                           Graphics& backend);

    // Buffer contains readback data from the GPU.
    // This data is aligned according to Vex internal alignment
    Buffer buffer;
    std::vector<TextureRegion> textureRegions;
    TextureDesc textureDesc;

    NonNullPtr<Graphics> backend;
    friend class CommandContext;
};
} // namespace vex
