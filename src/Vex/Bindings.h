#pragma once

#include <span>
#include <variant>

#include <Vex/Buffer.h>
#include <Vex/EnumFlags.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

namespace vex
{

struct ConstantBinding
{
    template <typename T>
    ConstantBinding(const T& data)
        : data{ static_cast<const void*>(&data) }
        , size{ sizeof(T) }
    {
    }

    const void* data;
    u32 size;

    // Concatenates all constant binding data into the writeableRange passed in as a parameter.
    // Returns the total byte size of all bindings concatenated together.
    static u32 ConcatConstantBindings(std::span<const ConstantBinding> constantBindings, std::span<u8> writableRange);
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
    // Name of the resource used inside the shader.
    // eg: VEX_RESOURCE(Texture2D<float3>, MyName);
    std::string name;
    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description
    BufferBindingUsage usage = BufferBindingUsage::Invalid;
    // Optional: Stride of the buffer when using StructuredBuffer usage
    u32 stride = 0;

    void ValidateForUse(BufferUsage::Flags validBufferUsageFlags) const;
    void Validate() const;
};

struct TextureBinding
{
    // Name of the resource used inside the shader.
    // eg: VEX_RESOURCE(Texture2D<float3>, MyName);
    std::string name;
    // The texture to bind
    Texture texture;
    TextureBindingUsage usage = TextureBindingUsage::Invalid;
    TextureBindingFlags::Flags flags = TextureBindingFlags::None;

    u32 mipBias = 0;
    // 0 means to use every mip.
    u32 mipCount = 0;

    u32 startSlice = 0;
    // 0 means to use every slice.
    u32 sliceCount = 0;

    void ValidateForUse(TextureUsage::Flags validTextureUsageFlags) const;
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

    void Validate() const;
};

} // namespace vex