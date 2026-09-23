#include "Bindings.h"

#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/Utility/Formattable.h>
#include <VexMacros.h>

namespace vex
{
static constexpr u32 ByteAddressBufferOffsetMultiple = 16;
static constexpr u32 ConstantBufferBindingOffsetMultiple = 256;

namespace BindingUtil
{

void ValidateBufferBinding(const BufferBinding& binding, Flags<BufferUsage> validBufferUsageFlags)
{
    const auto& buffer = binding.buffer;
    const auto& usage = binding.usage;

    VEX_CHECK(buffer.desc.usage & validBufferUsageFlags,
              "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
              "operation. Check the usage flags of your resource at creation.",
              buffer.desc.name);

    VEX_CHECK(IsBindingUsageCompatibleWithBufferUsage(buffer.desc.usage, usage),
              "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
              "usage.",
              buffer.desc.name);

    VEX_CHECK(usage != BufferBindingUsage::Invalid,
              "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
              "be invalid",
              buffer.desc.name);

    if (usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::RWStructuredBuffer)
    {
        VEX_CHECK(binding.strideByteSize.has_value(),
                  "Invalid binding for resource \"{}\": In order to use a binding as a structured buffer, you must "
                  "pass in a valid stride.",
                  buffer.desc.name);

        VEX_CHECK(*binding.strideByteSize > 0,
                  "Invalid binding for resource \"{}\": Stride for structured buffers must not be 0.",
                  buffer.desc.name);

        u64 offsetByteSize = binding.offsetByteSize.value_or(0);
        VEX_CHECK(offsetByteSize % *binding.strideByteSize == 0,
                  "Invalid binding for resource \"{}\": Offset must be a multiple of the stride.",
                  buffer.desc.name);

        VEX_CHECK(
            binding.rangeByteSize.value_or(binding.buffer.desc.byteSize - offsetByteSize) % *binding.strideByteSize ==
                0,
            "Invalid binding for resource \"{}\": Range must be a multiple of the stride.",
            buffer.desc.name);
    }

    if (usage == BufferBindingUsage::UniformBuffer)
    {
        VEX_CHECK(binding.offsetByteSize.value_or(0) % ConstantBufferBindingOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "Constant buffer offsets must be a multiple of 256 bytes",
                  buffer.desc.name);
    }

    if (usage == BufferBindingUsage::ByteAddressBuffer || usage == BufferBindingUsage::RWByteAddressBuffer)
    {
        VEX_CHECK(binding.offsetByteSize.value_or(0) % ByteAddressBufferOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "ByteAddressBuffer offsets must be a multiple of {} bytes (elements are {} bytes wide)",
                  buffer.desc.name,
                  ByteAddressBufferOffsetMultiple,
                  ByteAddressBufferOffsetMultiple);

        VEX_CHECK(binding.rangeByteSize.value_or(0) % ByteAddressBufferOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "ByteAddressBuffer range must be a multiple of {} bytes (elements are {} bytes wide)",
                  buffer.desc.name,
                  ByteAddressBufferOffsetMultiple,
                  ByteAddressBufferOffsetMultiple);
    }
}

void ValidateIndexBufferBinding(const IndexBufferBinding& binding)
{
    const auto& buffer = binding.buffer;

    VEX_CHECK(binding.offset < buffer.desc.byteSize,
              "Invalid binding for index buffer \"{}\": Buffer cannot have an offset larger than the buffer size.",
              buffer.desc.name);

    VEX_CHECK(
        buffer.desc.usage.IsSet(BufferUsage::IndexBuffer),
        "Invalid binding for index buffer \"{}\": Buffer must have the usage BufferUsage::IndexBuffer at creation.",
        buffer.desc.name);
}

void ValidateTextureBinding(const TextureBinding& binding, Flags<TextureUsage> validTextureUsageFlags)
{
    const auto& texture = binding.texture;

    VEX_CHECK(texture.desc.usage & validTextureUsageFlags,
              "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
              "operation. Check the usage flags of your resource at creation.",
              texture.desc.name);

    VEX_CHECK(binding.usage != TextureBindingUsage::None,
              "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
              "be invalid",
              texture.desc.name);

    TextureUtil::ValidateSubresource(texture.desc, binding.subresource);

    if (binding.isSRGB)
    {
        VEX_CHECK(FormatUtil::HasSRGBEquivalent(texture.desc.format),
                  "Invalid binding for resource \"{}\": Texture's format ({}) does not allow for an SRGB binding.",
                  texture.desc.name,
                  texture.desc.format);

        VEX_CHECK(binding.usage != TextureBindingUsage::ShaderReadWrite,
                  "Invalid binding for resource \"{}\": ShaderReadWrite usage cannot be SRGB! This is an API "
                  "limitation, use a non-SRGB binding and convert manually or write to the texture as a RenderTarget "
                  "in order to have SRGB conversion handled automatically.",
                  texture.desc.name);
    }

    VEX_CHECK(!FormatUtil::IsDepthOrDepthStencilFormat(texture.desc.format) ||
                  texture.desc.usage & TextureUsage::DepthStencil,
              "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
              "upon creation.",
              texture.desc.name,
              texture.desc.format);

    VEX_CHECK(TextureUtil::IsBindingUsageCompatibleWithUsage(texture.desc.usage, binding.usage),
              "Invalid binding for resource \"{}\": Binding usage must be compatible with texture description's"
              "usage.",
              texture.desc.name);
}

void ValidateRenderTargetBinding(const RenderTargetBinding& binding)
{
    const auto& texture = binding.texture;

    TextureUtil::ValidateSubresource(texture.desc, binding.subresource);

    VEX_CHECK(
        binding.subresource.mipCount == 1,
        "Invalid render target binding for texture \"{}\": Texture subresource cannot have a mip count different to 1.",
        texture.desc.name,
        texture.desc.format);

    VEX_CHECK(
        !binding.isSRGB || FormatUtil::HasSRGBEquivalent(texture.desc.format),
        "Invalid render target binding for texture \"{}\": Texture format ({}) does not allow for an SRGB binding.",
        texture.desc.name,
        texture.desc.format);
}

void ValidateDepthStencilBinding(const DepthStencilBinding& binding)
{
    const auto& texture = binding.texture;

    VEX_CHECK(FormatUtil::IsDepthOrDepthStencilFormat(texture.desc.format),
              "Invalid depth stencil binding for texture \"{}\": Texture cannot be bound as depth stencil due to it "
              "not having a depth or depth-stencil format.",
              texture.desc.name);

    VEX_CHECK(texture.desc.usage & TextureUsage::DepthStencil,
              "Invalid depth stencil binding for texture \"{}\": Texture format ({}) requires the depth stencil "
              "usage upon creation.",
              texture.desc.name,
              texture.desc.format);
}

void ValidateDrawResource(const DrawResourceBinding& binding)
{
    VEX_CHECK(binding.renderTargets.size() <= GMaxSimultaneousRenderTargetCount,
              "Cannot bind more than 8 render targets simultaneously.");
    for (const RenderTargetBinding& rt : binding.renderTargets)
    {
        ValidateRenderTargetBinding(rt);
    }

    if (binding.depthStencil)
    {
        ValidateDepthStencilBinding(*binding.depthStencil);
    }

    ValidateIndexBufferBinding(*binding.indexBuffer);
}

} // namespace BindingUtil

BufferBinding BufferBinding::CreateStructured(const Buffer& buffer,
                                              u32 strideByteSize,
                                              u32 firstElement,
                                              std::optional<u32> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::StructuredBuffer,
        .strideByteSize = strideByteSize,
        .offsetByteSize = static_cast<u64>(firstElement) * static_cast<u64>(strideByteSize),
        .rangeByteSize = elementCount.value_or(buffer.desc.byteSize / strideByteSize - firstElement) * strideByteSize,
    };
}

BufferBinding BufferBinding::CreateRWStructured(const Buffer& buffer,
                                                u32 strideByteSize,
                                                u32 firstElement,
                                                std::optional<u32> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::RWStructuredBuffer,
        .strideByteSize = strideByteSize,
        .offsetByteSize = firstElement * strideByteSize,
        .rangeByteSize = elementCount.value_or((buffer.desc.byteSize / strideByteSize) - firstElement) * strideByteSize,
    };
}

BufferBinding BufferBinding::CreateRWByteAddress(const Buffer& buffer,
                                                 u32 firstElement,
                                                 std::optional<u64> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::RWByteAddressBuffer,
        .offsetByteSize = firstElement * ByteAddressBufferOffsetMultiple,
        .rangeByteSize = elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                         ByteAddressBufferOffsetMultiple,
    };
}

BufferBinding BufferBinding::CreateByteAddress(const Buffer& buffer, u32 firstElement, std::optional<u64> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::ByteAddressBuffer,
        .offsetByteSize = firstElement * ByteAddressBufferOffsetMultiple,
        .rangeByteSize = elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                         ByteAddressBufferOffsetMultiple,
    };
}

BufferBinding BufferBinding::CreateUniform(const Buffer& buffer, u32 offsetByteSize, std::optional<u64> rangeByteSize)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::UniformBuffer,
        .offsetByteSize = offsetByteSize,
        .rangeByteSize = rangeByteSize.value_or(buffer.desc.byteSize - offsetByteSize),
    };
}

} // namespace vex
