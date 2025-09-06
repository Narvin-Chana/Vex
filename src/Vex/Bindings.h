#pragma once

#include <optional>
#include <span>
#include <variant>

#include <Vex/Buffer.h>
#include <Vex/Concepts.h>
#include <Vex/EnumFlags.h>
#include <Vex/RHIFwd.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

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
        : ConstantBinding(static_cast<const void*>(data.data()), data.size_bytes())
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
    std::optional<u64> offsetByteSize;

    void ValidateForShaderUse(BufferUsage::Flags validBufferUsageFlags) const;
    void Validate() const;
};

struct TextureBinding
{
    // The texture to bind
    Texture texture;
    // The usage of the texture.
    TextureBindingUsage usage = TextureBindingUsage::None;
    TextureBindingFlags::Flags flags = TextureBindingFlags::None;

    u32 mipBias = 0;
    // 0 means to use every mip.
    u32 mipCount = 0;

    u32 startSlice = 0;
    // 0 means to use every slice.
    u32 sliceCount = 0;

    void ValidateForShaderUse(TextureUsage::Flags validTextureUsageFlags) const;
    void Validate() const;
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

    void Validate() const;
};

} // namespace vex