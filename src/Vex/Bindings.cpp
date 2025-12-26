#include "Bindings.h"

#include <numeric>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>
#include <Vex/Utility/Validation.h>

namespace vex
{
static constexpr u32 ByteAddressBufferOffsetMultiple = 16;
static constexpr u32 ConstantBufferBindingOffsetMultiple = 256;

namespace BindingUtil
{

void ValidateBufferBinding(const BufferBinding& binding, BufferUsage::Flags validBufferUsageFlags)
{
    const auto& buffer = binding.buffer;
    const auto& usage = binding.usage;
    if (!(buffer.desc.usage & validBufferUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                buffer.desc.name);
    }

    if (!IsBindingUsageCompatibleWithBufferUsage(buffer.desc.usage, usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
                "usage.",
                buffer.desc.name);
    }

    if (usage == BufferBindingUsage::Invalid)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                buffer.desc.name);
    }

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

    if (usage == BufferBindingUsage::ConstantBuffer)
    {
        VEX_CHECK(binding.offsetByteSize.value_or(0) % ConstantBufferBindingOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "Constant buffer offsets must be a multiple of 256 bytes",
                  buffer.desc.name)
    }

    if (usage == BufferBindingUsage::ByteAddressBuffer || usage == BufferBindingUsage::RWByteAddressBuffer)
    {
        VEX_CHECK(binding.offsetByteSize.value_or(0) % ByteAddressBufferOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "ByteAddressBuffer offsets must be a multiple of {} bytes (elements are {} bytes wide)",
                  buffer.desc.name,
                  ByteAddressBufferOffsetMultiple,
                  ByteAddressBufferOffsetMultiple)

        VEX_CHECK(binding.rangeByteSize.value_or(0) % ByteAddressBufferOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "ByteAddressBuffer range must be a multiple of {} bytes (elements are {} bytes wide)",
                  buffer.desc.name,
                  ByteAddressBufferOffsetMultiple,
                  ByteAddressBufferOffsetMultiple)
    }
}

void ValidateTextureBinding(const TextureBinding& binding, TextureUsage::Flags validTextureUsageFlags)
{
    const auto& texture = binding.texture;
    if (!(texture.desc.usage & validTextureUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                texture.desc.name);
    }

    if ((validTextureUsageFlags & TextureUsage::DepthStencil) &&
        !FormatUtil::IsDepthOrStencilFormat(texture.desc.format))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": texture cannot be bound as depth stencil",
                texture.desc.name);
    }

    if (binding.usage == TextureBindingUsage::None)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                texture.desc.name);
    }

    TextureUtil::ValidateSubresource(texture.desc, binding.subresource);

    if (binding.isSRGB)
    {
        if (!FormatUtil::HasSRGBEquivalent(texture.desc.format))
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": Texture's format ({}) does not allow for an SRGB binding.",
                    texture.desc.name,
                    texture.desc.format);
        }

        if (binding.usage == TextureBindingUsage::ShaderReadWrite)
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": ShaderReadWrite usage cannot be SRGB! This is an API "
                    "limitation, use a non-SRGB binding and convert manually or write to the texture as a RenderTarget "
                    "in order to have SRGB conversion handled automatically.",
                    texture.desc.name);
        }
    }

    if (FormatUtil::IsDepthOrStencilFormat(texture.desc.format) && !(texture.desc.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                "upon creation.",
                texture.desc.name,
                texture.desc.format);
    }

    if (!TextureUtil::IsBindingUsageCompatibleWithUsage(texture.desc.usage, binding.usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with texture description's"
                "usage.",
                texture.desc.name);
    }
}

void ValidateDrawResource(const DrawResourceBinding& binding)
{
    for (const auto& binding : binding.renderTargets)
    {
        ValidateTextureBinding(binding, TextureUsage::RenderTarget);
    }

    if (binding.depthStencil)
    {
        ValidateTextureBinding(*binding.depthStencil, TextureUsage::DepthStencil);
    }
}

} // namespace BindingUtil

BufferBinding BufferBinding::CreateStructuredBuffer(const Buffer& buffer,
                                                    u32 strideByteSize,
                                                    u32 firstElement,
                                                    std::optional<u32> elementCount)
{
    return { .buffer = buffer,
             .usage = BufferBindingUsage::StructuredBuffer,
             .strideByteSize = strideByteSize,
             .offsetByteSize = firstElement * strideByteSize,
             .rangeByteSize =
                 elementCount.value_or((buffer.desc.byteSize / strideByteSize) - firstElement) * strideByteSize };
}

BufferBinding BufferBinding::CreateRWStructuredBuffer(const Buffer& buffer,
                                                      u32 strideByteSize,
                                                      u32 firstElement,
                                                      std::optional<u32> elementCount)
{
    return { .buffer = buffer,
             .usage = BufferBindingUsage::RWStructuredBuffer,
             .strideByteSize = strideByteSize,
             .offsetByteSize = firstElement * strideByteSize,
             .rangeByteSize =
                 elementCount.value_or((buffer.desc.byteSize / strideByteSize) - firstElement) * strideByteSize };
}

BufferBinding BufferBinding::CreateRWByteAddressBuffer(const Buffer& buffer,
                                                       u32 firstElement,
                                                       std::optional<u64> elementCount)
{
    return { .buffer = buffer,
             .usage = BufferBindingUsage::RWByteAddressBuffer,
             .offsetByteSize = firstElement * ByteAddressBufferOffsetMultiple,
             .rangeByteSize =
                 elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                 ByteAddressBufferOffsetMultiple };
}

BufferBinding BufferBinding::CreateByteAddressBuffer(const Buffer& buffer,
                                                     u32 firstElement,
                                                     std::optional<u64> elementCount)
{
    return { .buffer = buffer,
             .usage = BufferBindingUsage::ByteAddressBuffer,
             .offsetByteSize = firstElement * ByteAddressBufferOffsetMultiple,
             .rangeByteSize =
                 elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                 ByteAddressBufferOffsetMultiple };
}

BufferBinding BufferBinding::CreateConstantBuffer(const Buffer& buffer,
                                                  u32 offsetByteSize,
                                                  std::optional<u64> rangeByteSize)
{
    return { .buffer = buffer,
             .usage = BufferBindingUsage::ConstantBuffer,
             .offsetByteSize = offsetByteSize,
             .rangeByteSize = rangeByteSize.value_or(buffer.desc.byteSize - offsetByteSize) };
}
} // namespace vex
