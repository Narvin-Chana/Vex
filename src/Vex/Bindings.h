#pragma once

#include <span>

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

// Flags for a buffer binding.
BEGIN_VEX_ENUM_FLAGS(BufferBinding, u8)
    None,
END_VEX_ENUM_FLAGS();

// Flags for a texture binding.
BEGIN_VEX_ENUM_FLAGS(TextureBinding, u8)
    None,
    SRGB = 1,
END_VEX_ENUM_FLAGS();

// clang-format on

struct ResourceBinding
{
    // Name of the resource used inside the shader.
    // eg: VEX_RESOURCE(Texture2D<float3>, MyName);
    std::string name;

    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description
    BufferBindingUsage bufferUsage;

    // The texture to bind
    Texture texture;
    // Flags for binding the texture
    TextureBinding::Flags textureFlags;

    u32 mipBias = 0;
    // 0 means to use every mip.
    u32 mipCount = 0;

    u32 startSlice = 0;
    // 0 means to use every slice.
    u32 sliceCount = 0;

    // Stride of the buffer if necessary
    u32 bufferStride = 0;

    [[nodiscard]] bool IsBuffer() const
    {
        return buffer.handle != GInvalidBufferHandle;
    }
    [[nodiscard]] bool IsTexture() const
    {
        return texture.handle != GInvalidTextureHandle;
    }

    // Takes in an array of bindings and does some validation on them
    // the passed in flags assure that the bindings can be used according to those flags
    static void ValidateResourceBindings(std::span<const ResourceBinding> bindings,
                                         TextureUsage::Flags validUsageFlags,
                                         BufferUsage::Flags validBufferUsageFlags = BufferUsage::GenericBuffer);
};

} // namespace vex