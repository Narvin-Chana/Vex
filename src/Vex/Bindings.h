#pragma once

#include <optional>
#include <span>
#include <variant>

#include <Vex/Buffer.h>
#include <Vex/Concepts.h>
#include <Vex/EnumFlags.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct ConstantBinding
{
    explicit ConstantBinding(const void* data, std::span<const byte>::size_type size)
        : data{ reinterpret_cast<const byte*>(data), size }
    {
    }

    template <typename T>
    explicit ConstantBinding(std::span<T> data)
        : data(std::as_bytes(data))
    {
    }

    // Avoids this constructor taking in a container, and thus polluting constant data with the container's data (eg: a
    // vector's size/capacity).
    template <typename T>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize and not IsContainer<T>)
    explicit ConstantBinding(const T& data)
        : ConstantBinding(static_cast<const void*>(&data), sizeof(T))
    {
    }

    std::span<const byte> data;
};

// clang-format off

// Flags for a texture binding.
BEGIN_VEX_ENUM_FLAGS(TextureBindingFlags, u8)
    None,
    SRGB = 1,
END_VEX_ENUM_FLAGS();

// clang-format on

struct BufferBinding
{
    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description
    BufferBindingUsage usage = BufferBindingUsage::Invalid;

    // Optional: Stride of the buffer in bytes when using StructuredBuffer usage
    std::optional<u32> strideByteSize;
    // Optional: The offset to apply when binding the buffer (in bytes).
    // There are some limitations when using ConstantBuffer usage
    std::optional<u64> offsetByteSize;
    // Optional: The range in bytes starting from the offset to bind.
    // If not specified the remaining range past the offset is bound
    std::optional<u64> rangeByteSize;

    // firstElement and elementCount represent strideByteSize multiples on the buffer
    static BufferBinding CreateStructuredBuffer(const Buffer& buffer,
                                                u32 strideByteSize,
                                                u32 firstElement = 0,
                                                std::optional<u32> elementCount = {});

    // firstElement and elementCount represent strideByteSize multiples on the buffer
    static BufferBinding CreateRWStructuredBuffer(const Buffer& buffer,
                                                  u32 strideByteSize,
                                                  u32 firstElement = 0,
                                                  std::optional<u32> elementCount = {});

    // First element and element count represent 16 byte elements on the ByteAddressBuffer
    // example: FirstElement = 1, ElementCount = 10 represents a view on bytes [16, 176] in the buffer
    // example: FirstElement = 0, ElementCount = 2 represents a view on bytes [0, 32] in the buffer
    static BufferBinding CreateByteAddressBuffer(const Buffer& buffer,
                                                 u32 firstElement = 0,
                                                 std::optional<u64> elementCount = {});

    static BufferBinding CreateRWByteAddressBuffer(const Buffer& buffer,
                                                   u32 firstElement = 0,
                                                   std::optional<u64> elementCount = {});

    // offsetByteSize must be a multiple of 128 bytes
    static BufferBinding CreateConstantBuffer(const Buffer& buffer,
                                              u32 offsetByteSize = 0,
                                              std::optional<u64> rangeByteSize = {});
};

struct TextureBinding
{
    // The texture to bind.
    Texture texture;
    // The usage of the texture.
    TextureBindingUsage usage = TextureBindingUsage::None;
    // Special flags for the texture binding.
    TextureBindingFlags::Flags flags = TextureBindingFlags::None;
    // Subresource of the texture, defaults to all mips and all slices (so the entirety of the resource).
    TextureSubresource subresource;
};

struct ResourceBinding
{
    ResourceBinding(const TextureBinding& binding)
        : binding{ binding } {};
    ResourceBinding(const BufferBinding& binding)
        : binding{ binding } {};

    std::variant<TextureBinding, BufferBinding> binding;

    [[nodiscard]] bool IsTexture() const
    {
        return std::holds_alternative<TextureBinding>(binding);
    }
    [[nodiscard]] const TextureBinding& GetTextureBinding() const
    {
        return std::get<TextureBinding>(binding);
    }

    [[nodiscard]] bool IsBuffer() const
    {
        return std::holds_alternative<BufferBinding>(binding);
    }
    [[nodiscard]] const BufferBinding& GetBufferBinding() const
    {
        return std::get<BufferBinding>(binding);
    }
};

struct DrawResourceBinding
{
    std::span<const TextureBinding> renderTargets;
    std::optional<const TextureBinding> depthStencil;

    u32 vertexBuffersFirstSlot = 0;
    // Vertex buffers to be bound starting at the above slot.
    // You can bind no vertex buffer and instead depend on SV_VertexID in your Vertex Shader.
    std::span<BufferBinding> vertexBuffers;

    // Index buffer used for DrawIndexed.
    std::optional<BufferBinding> indexBuffer;
};

namespace BindingUtil
{

void ValidateBufferBinding(const BufferBinding& binding, BufferUsage::Flags validBufferUsageFlags);
void ValidateTextureBinding(const TextureBinding& binding, TextureUsage::Flags validTextureUsageFlags);
void ValidateDrawResource(const DrawResourceBinding& binding);

} // namespace BindingUtil

} // namespace vex