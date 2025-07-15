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

    static std::vector<u8> ConcatConstantBindings(std::span<const ConstantBinding> constantBindings, u32 maxBufferSize);
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

    Buffer buffer;
    BufferBinding::Flags bufferFlags;

    Texture texture;
    TextureBinding::Flags textureFlags;

    u32 mipBias = 0;
    // 0 means to use every mip.
    u32 mipCount = 0;

    u32 startSlice = 0;
    // 0 means to use every slice.
    u32 sliceCount = 0;

    [[nodiscard]] bool IsBuffer() const
    {
        return buffer.handle != GInvalidBufferHandle;
    }
    [[nodiscard]] bool IsTexture() const
    {
        return texture.handle != GInvalidTextureHandle;
    }

    static void ValidateResourceBindings(std::span<const ResourceBinding> bindings,
                                         ResourceUsage::Flags validUsageFlags);
};

} // namespace vex