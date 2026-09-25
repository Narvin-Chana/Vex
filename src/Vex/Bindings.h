#pragma once

#include <optional>
#include <variant>

#include <Vex/AccelerationStructure.h>
#include <Vex/Buffer.h>
#include <Vex/Containers/Span.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/Concepts.h>
#include <Vex/Utility/EnumFlags.h>
#include <VexMacros.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct ConstantBinding
{
    constexpr ConstantBinding() = default;

    // Construct from raw ptr and size.
    constexpr explicit ConstantBinding(const void* data, Span<const byte>::size_type size)
        : data{ static_cast<const byte*>(data), size }
    {
        VEX_ASSERT(
            size <= MaxTheoreticalLocalConstantsByteSize,
            "Size cannot surpass the max theoretical limit for local constants as defined by your graphics api.");
    }

    // Construct from vex::Span.
    template <typename T, std::size_t N>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize)
    explicit ConstantBinding(Span<T, N> data)
        : data(std::as_bytes(data))
    {
    }

    // Construct from std::span.
    template <typename T, std::size_t N>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize)
    explicit ConstantBinding(std::span<T, N> data)
        : data(std::as_bytes(data))
    {
    }

    // Construct constant binding from any non-container T.
    // This constructor's concepts are here to avoid taking in a container, and thus polluting constant data with the
    // container's data (eg: a vector's size/capacity).
    template <typename T>
        requires(sizeof(T) <= MaxTheoreticalLocalConstantsByteSize and not IsContainer<T> and
                 std::is_trivially_copyable_v<T>)
    explicit ConstantBinding(const T& data)
        : ConstantBinding(static_cast<const void*>(&data), sizeof(T))
    {
    }

    [[nodiscard]] constexpr bool IsValid() const
    {
        return !data.empty();
    }

    Span<const byte> data;
};

struct BufferBinding
{
    // The buffer to bind
    Buffer buffer;
    // The usage to use in this binding. Needs to be part of the usages of the buffer description.
    BufferBindingUsage usage = BufferBindingUsage::Invalid;
    // Byte region to bind, defaults to the entire buffer.
    // Note for the offset field:
    //  - When using UniformBuffer usage the offset must be a multiple of 256 bytes
    //  - When using (RW)ByteAddressBuffer usage the offset must be a multiple of 16 bytes.
    // Note for the byteSize field:
    //  - When using (RW)ByteAddressBuffer usage the range must be a multiple of 16 bytes
    BufferRegion region;
    // Optional: Stride of the buffer in bytes, required when using (RW)StructuredBuffer usage.
    std::optional<u32> strideByteSize;

    // firstElement and elementCount represent strideByteSize multiples on the buffer
    static BufferBinding CreateStructured(const Buffer& buffer,
                                          u64 strideByteSize,
                                          u64 firstElement = 0,
                                          std::optional<u64> elementCount = {});

    template <class T>
    static BufferBinding CreateStructured(const Buffer& buffer,
                                          u64 firstElement = 0,
                                          std::optional<u64> elementCount = {});

    // firstElement and elementCount represent strideByteSize multiples on the buffer
    static BufferBinding CreateRWStructured(const Buffer& buffer,
                                            u64 strideByteSize,
                                            u64 firstElement = 0,
                                            std::optional<u64> elementCount = {});

    // First element and element count represent 16 byte elements on the ByteAddressBuffer
    // example: FirstElement = 1, ElementCount = 10 represents a view on bytes [16, 176] in the buffer
    // example: FirstElement = 0, ElementCount = 2 represents a view on bytes [0, 32] in the buffer
    static BufferBinding CreateByteAddress(const Buffer& buffer,
                                           u64 firstElement = 0,
                                           std::optional<u64> elementCount = {});

    static BufferBinding CreateRWByteAddress(const Buffer& buffer,
                                             u64 firstElement = 0,
                                             std::optional<u64> elementCount = {});

    // offsetByteSize must be a multiple of 128 bytes
    static BufferBinding CreateUniform(const Buffer& buffer,
                                       u64 offsetByteSize = 0,
                                       std::optional<u64> rangeByteSize = {});
};

template <class T>
BufferBinding BufferBinding::CreateStructured(const Buffer& buffer, u64 firstElement, std::optional<u64> elementCount)
{
    static_assert(sizeof(T) <= std::numeric_limits<u32>::max(), "Type must be smaller than MAX_U32.");
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::StructuredBuffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * sizeof(T),
                .byteSize = elementCount.value_or(buffer.desc.byteSize / sizeof(T) - firstElement) * sizeof(T),
            },
        .strideByteSize = static_cast<u32>(sizeof(T)),
    };
}

struct IndexBufferBinding
{
    // The buffer to bind.
    Buffer buffer;
    // Region of the index buffer to bind.
    BufferRegion region;
    // Format of the index.
    IndexFormat format = IndexFormat::U32;

    static IndexBufferBinding Create(const Buffer& buffer,
                                     IndexFormat format,
                                     u64 firstElement = 0,
                                     std::optional<u64> elementCount = std::nullopt);
    template <class T>
    static IndexBufferBinding Create(const Buffer& buffer,
                                     u64 firstElement = 0,
                                     std::optional<u64> elementCount = std::nullopt);
};

template <class T>
IndexBufferBinding IndexBufferBinding::Create(const Buffer& buffer, u64 firstElement, std::optional<u64> elementCount)
{
    static_assert(sizeof(T) == sizeof(u16) || sizeof(T) == sizeof(u32), "Type must be a supported index type.");
    return {
        .buffer = buffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * sizeof(T),
                .byteSize = elementCount ? *elementCount * sizeof(T) : GBufferWholeSize,
            },
        .format = static_cast<IndexFormat>(sizeof(T)),
    };
}

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
    // Force view type of the texture to another type (eg: TextureCube to Texture2DArray (or Texture2D if only one slice
    // in subresource)).
    std::optional<TextureViewType> viewTypeOverride = std::nullopt;
};

struct RenderTargetBinding
{
    // The texture to bind.
    Texture texture;
    // Subresource of the texture, defaults to the first mip and the first slice.
    TextureSubresource subresource{ .startMip = 0, .mipCount = 1, .startSlice = 0, .sliceCount = 1 };
    // Determines if the render target output should use the hardware SRGB format.
    bool isSRGB = false;
};

struct DepthStencilBinding
{
    // The texture to bind.
    Texture texture;
    // Subresource of the texture, defaults to the first mip and the first slice.
    TextureSubresource subresource{ .startMip = 0, .mipCount = 1, .startSlice = 0, .sliceCount = 1 };
};

using AccelerationStructureBinding = AccelerationStructure;

struct ResourceBinding
{
    ResourceBinding(const TextureBinding& binding)
        : binding{ binding }
    {
    }
    ResourceBinding(const BufferBinding& binding)
        : binding{ binding }
    {
    }
    ResourceBinding(const AccelerationStructureBinding& binding)
        : binding{ binding }
    {
    }

    std::variant<TextureBinding, BufferBinding, AccelerationStructureBinding> binding;

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

    [[nodiscard]] bool IsAccelerationStructure() const
    {
        return std::holds_alternative<AccelerationStructureBinding>(binding);
    }
    [[nodiscard]] const AccelerationStructureBinding& GetAccelerationStructureBinding() const
    {
        return std::get<AccelerationStructureBinding>(binding);
    }
};

struct DrawResourceBinding
{
    // Which textures to render-to.
    Span<const RenderTargetBinding> renderTargets;
    // Depth(-stencil) buffer to use for depth testing (optional).
    const DepthStencilBinding* depthStencil = nullptr;
    // Index buffer used for DrawIndexed (optional).
    const IndexBufferBinding* indexBuffer = nullptr;
};

namespace BindingUtil
{

void ValidateBufferBinding(const BufferBinding& binding, Flags<BufferUsage> validBufferUsageFlags);
void ValidateIndexBufferBinding(const IndexBufferBinding& binding);
void ValidateTextureBinding(const TextureBinding& binding, Flags<TextureUsage> validTextureUsageFlags);
void ValidateRenderTargetBinding(const RenderTargetBinding& binding);
void ValidateDepthStencilBinding(const DepthStencilBinding& binding);
void ValidateDrawResource(const DrawResourceBinding& binding);

} // namespace BindingUtil

} // namespace vex