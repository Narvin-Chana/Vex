#pragma once

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
        : data{ &data }
        , size{ sizeof(T) }
    {
    }

    void* data;
    size_t size;
};

// clang-format off

// Flags for a buffer binding.
BEGIN_VEX_ENUM_FLAGS(BufferBinding, u8)
    None,
END_VEX_ENUM_FLAGS();

// Flags for a texture binding.
BEGIN_VEX_ENUM_FLAGS(TextureBinding, u8)
    None,
    Read_SRGB,
END_VEX_ENUM_FLAGS();

// clang-format on

struct ResourceBinding
{
    u32 slot;

    Buffer buffer;
    BufferBinding::Flags bufferFlags;

    Texture texture;
    TextureBinding::Flags textureFlags;

    bool IsBuffer() const
    {
        return buffer.handle != GInvalidBufferHandle;
    }
    bool IsTexture() const
    {
        return texture.handle != GInvalidTextureHandle;
    }
};
} // namespace vex