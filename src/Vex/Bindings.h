#pragma once

#include <optional>
#include <span>
#include <variant>

#include <Vex/Buffer.h>
#include <Vex/EnumFlags.h>
#include <Vex/RHIFwd.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

namespace vex
{

struct ConstantBinding
{
    template <typename T>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize)
    ConstantBinding(std::string typeName, const T& data)
        : typeName(std::move(typeName))
        , data{ static_cast<const void*>(&data) }
        , size{ sizeof(T) }
    {
    }

    // Shader struct type to use for the constant binding.
    std::string typeName;

    // Constant binding data.
    const void* data;

    // Byte size of the local constant binding.
    u32 size;
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
    std::string name;
    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description
    BufferBindingUsage usage = BufferBindingUsage::Invalid;
    // Optional: The name of the underlying shader type, only required for typed buffers such as StructuredBuffer or
    // ConstantBuffer.
    std::optional<std::string> typeName;
    // Optional: Stride of the buffer when using StructuredBuffer usage
    std::optional<u32> stride = 0;

    void ValidateForShaderUse(BufferUsage::Flags validBufferUsageFlags) const;
    void Validate() const;
};

struct TextureBinding
{
    // Name of the resource used inside the shader.
    std::string name;
    // The texture to bind
    Texture texture;
    // The usage of the texture.
    TextureBindingUsage usage = TextureBindingUsage::None;
    // The underlying shader type to use for the texture.
    // Only valid HLSL/Slang primitives are accepted.
    // Not passing a texture typename will default to float4.
    // eg: "float4" for an RGBA texture, "uint" for a R8_UINT texture, etc...
    std::string typeName = "float4";
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

    void Validate() const;
};

} // namespace vex