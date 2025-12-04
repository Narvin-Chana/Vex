#pragma once

#include <optional>
#include <Vex/Containers/Span.h>
#include <variant>

#include <Vex/Buffer.h>
#include <Vex/Utility/Concepts.h>
#include <Vex/Utility/EnumFlags.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct ConstantBinding
{
    constexpr ConstantBinding() = default;

    // Construct from raw ptr and size.
    constexpr explicit ConstantBinding(const void* data, Span<const byte>::size_type size)
        : data{ reinterpret_cast<const byte*>(data), size }
    {
    }

    // Construct from vex::Span.
    template <typename T>
    explicit ConstantBinding(Span<T> data)
        : data(std::as_bytes(data))
    {
    }

    // Construct from std::span.
    template <typename T>
    explicit ConstantBinding(std::span<T> data)
        : data(std::as_bytes(data))
    {
    }

    // Construct constant binding from any non-container T.
    // This constructor's concepts are here to avoid taking in a container, and thus polluting constant data with the container's data (eg: a
    // vector's size/capacity).
    template <typename T>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize and not IsContainer<T>)
    explicit ConstantBinding(const T& data)
        : ConstantBinding(static_cast<const void*>(&data), sizeof(T))
    {
    }

    constexpr bool IsValid() const
    {
        return !data.empty();
    }

    Span<const byte> data;
};

struct BufferBinding
{
    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description
    BufferBindingUsage usage = BufferBindingUsage::Invalid;

    // Optional: Stride of the buffer in bytes when using StructuredBuffer usage
    std::optional<u32> strideByteSize;

    // Optional: The offset to apply when binding the buffer (in bytes).
    // When using ConstantBuffer usage the offset must be a multiple of 256 bytes
    // When using (RW)ByteAddressBuffer usage the offset must be a multiple of 16 bytes
    std::optional<u64> offsetByteSize;

    // Optional: The range in bytes starting from the offset to bind.
    // If not specified the remaining range past the offset is bound
    // When using (RW)ByteAddressBuffer usage the range must be a multiple of 16 bytes
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
    // Determines if the texture should be sampled as an SRGB format (will NOT work with ShaderReadWrite usage).
    bool isSRGB = false;
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
    Span<const TextureBinding> renderTargets;
    std::optional<const TextureBinding> depthStencil;

    u32 vertexBuffersFirstSlot = 0;
    // Vertex buffers to be bound starting at the above slot.
    // You can bind no vertex buffer and instead depend on SV_VertexID in your Vertex Shader.
    Span<const BufferBinding> vertexBuffers;

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