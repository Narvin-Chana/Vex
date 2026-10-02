#include "Bindings.h"

#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/Utility/Formattable.h>
#include <VexMacros.h>

namespace vex
{
static constexpr u64 ByteAddressBufferOffsetMultiple = 16;
static constexpr u64 ConstantBufferBindingOffsetMultiple = 256;

namespace Bindings_Internal
{

static void ValidateRTDSBinding(const TextureDesc& desc, u16 mip, u32 startDepthSlice, u32 depthSliceCount)
{
    VEX_CHECK(mip < desc.mips,
              "Invalid binding for texture \"{}\": Cannot bind a mip ({}) greater than the actual "
              "texture's mip "
              "count ({}).",
              desc.name,
              mip,
              desc.mips);

    VEX_CHECK(depthSliceCount > 0,
              "Invalid binding for texture \"{}\": Must bind at least one depth slice...",
              desc.name);

    if (desc.type != TextureType::Texture3D)
    {
        VEX_CHECK(startDepthSlice < desc.GetSliceCount(),
                  "Invalid binding for texture \"{}\": The start slice ({}) cannot be larger than the "
                  "actual texture's array size ({}).",
                  desc.name,
                  startDepthSlice,
                  desc.GetSliceCount());
        VEX_CHECK(startDepthSlice + depthSliceCount <= desc.GetSliceCount(),
                  "Invalid binding for texture \"{}\": The subresource accesses more slices than "
                  "available, startDepthSlice: {}, depthSliceCount: {},  texture slice count {}",
                  desc.name,
                  startDepthSlice,
                  depthSliceCount,
                  desc.GetSliceCount());
    }
    else
    {
        const u32 mipDepth = std::get<2>(TextureUtil::GetMipSize(desc, mip));
        VEX_CHECK(startDepthSlice < mipDepth,
                  "Invalid binding for texture \"{}\": The start depth slice ({}) cannot be larger than the "
                  "actual texture's depth ({}) for mip ({}).",
                  desc.name,
                  startDepthSlice,
                  mipDepth,
                  mip);
        VEX_CHECK(startDepthSlice + depthSliceCount <= mipDepth,
                  "Invalid binding for texture \"{}\": The subresource accesses more depth slices than "
                  "available, startDepthSlice: {}, depthSliceCount: {},  texture depth {} for mip {}",
                  desc.name,
                  startDepthSlice,
                  depthSliceCount,
                  mipDepth,
                  mip);
    }
}

} // namespace Bindings_Internal

namespace BindingUtil
{

void ValidateBufferBinding(const BufferBinding& binding, Flags<BufferUsage> validBufferUsageFlags)
{
    const auto& buffer = binding.buffer;
    const auto& usage = binding.usage;

    BufferUtil::ValidateBufferRegion(buffer.desc, binding.region);

    VEX_CHECK(buffer.desc.usage & validBufferUsageFlags,
              "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
              "operation. Check the usage flags of your resource at creation.",
              buffer.desc.name);

    VEX_CHECK(IsBindingUsageCompatibleWithBufferUsage(buffer.desc.usage, usage),
              "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
              "usage.",
              buffer.desc.name);

    VEX_CHECK(usage != BufferBindingUsage::Invalid,
              "Invalid binding for resource \"{}\": The binding's usage must be set to something other than "
              "BufferBindingUsage::Invalid.",
              buffer.desc.name);

    if (usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::RWStructuredBuffer)
    {
        VEX_CHECK(binding.strideByteSize > 0,
                  "Invalid binding for resource \"{}\": Stride for structured buffers must be non-zero.",
                  buffer.desc.name);

        u64 offsetByteSize = binding.region.byteOffset;
        VEX_CHECK(offsetByteSize % binding.strideByteSize == 0,
                  "Invalid binding for resource \"{}\": Offset must be a multiple of the stride.",
                  buffer.desc.name);

        VEX_CHECK(binding.region.GetByteSize(buffer.desc) % binding.strideByteSize == 0,
                  "Invalid binding for resource \"{}\": Range must be a multiple of the stride.",
                  buffer.desc.name);
    }

    if (usage == BufferBindingUsage::UniformBuffer)
    {
        VEX_CHECK(binding.region.byteOffset % ConstantBufferBindingOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "Constant buffer offsets must be a multiple of 256 bytes",
                  buffer.desc.name);
    }

    if (usage == BufferBindingUsage::ByteAddressBuffer || usage == BufferBindingUsage::RWByteAddressBuffer)
    {
        VEX_CHECK(binding.region.byteOffset % ByteAddressBufferOffsetMultiple == 0,
                  "Invalid binding for resource \"{}\": "
                  "ByteAddressBuffer offsets must be a multiple of {} bytes (elements are {} bytes wide)",
                  buffer.desc.name,
                  ByteAddressBufferOffsetMultiple,
                  ByteAddressBufferOffsetMultiple);

        VEX_CHECK(binding.region.byteSize == GBufferWholeSize ||
                      binding.region.byteSize % ByteAddressBufferOffsetMultiple == 0,
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

    BufferUtil::ValidateBufferRegion(buffer.desc, binding.region);

    VEX_CHECK(
        buffer.desc.usage.IsSet(BufferUsage::IndexBuffer),
        "Invalid binding for index buffer \"{}\": Buffer must have the usage BufferUsage::IndexBuffer at creation.",
        buffer.desc.name);

    VEX_CHECK(binding.format == IndexFormat::U16 || binding.format == IndexFormat::U32,
              "Invalid binding for index buffer \"{}\": Buffer must have a valid index format (u16 or u32).",
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
    const auto& desc = texture.desc;
    Bindings_Internal::ValidateRTDSBinding(desc, binding.mip, binding.startDepthSlice, binding.depthSliceCount);
    VEX_CHECK(desc.usage.IsSet(TextureUsage::RenderTarget),
              "Invalid render target binding for texture \"{}\": Texture must have been created with flag "
              "TextureUsage::RenderTarget.",
              desc.name);

    VEX_CHECK(!FormatUtil::IsDepthOrDepthStencilFormat(desc.format),
              "Invalid render target binding for texture \"{}\": Depth-stencil formats must use a DepthStencilBinding.",
              desc.name);

    VEX_CHECK(
        !binding.isSRGB || FormatUtil::HasSRGBEquivalent(desc.format),
        "Invalid render target binding for texture \"{}\": Texture format ({}) does not allow for an SRGB binding.",
        desc.name,
        desc.format);
}

void ValidateDepthStencilBinding(const DepthStencilBinding& binding)
{
    const auto& texture = binding.texture;
    const auto& desc = texture.desc;
    Bindings_Internal::ValidateRTDSBinding(desc, binding.mip, binding.startDepthSlice, binding.depthSliceCount);

    VEX_CHECK(FormatUtil::IsDepthOrDepthStencilFormat(desc.format),
              "Invalid depth stencil binding for texture \"{}\": Texture cannot be bound as depth stencil due to it "
              "not having a depth or depth-stencil format.",
              desc.name);

    VEX_CHECK(desc.usage & TextureUsage::DepthStencil,
              "Invalid depth stencil binding for texture \"{}\": Texture format ({}) requires the depth stencil "
              "usage upon creation.",
              desc.name,
              desc.format);

    VEX_CHECK(binding.aspect.IsSet(TextureAspect::Depth) ||
                  (FormatUtil::IsDepthAndStencilFormat(desc.format) && binding.aspect.IsSet(TextureAspect::Stencil)),
              "Invalid depth stencil binding for texture \"{}\": Invalid aspect flags, a depth stencil binding can "
              "only have depth, stencil or depth-stencil aspects.",
              desc.name);
}

void ValidateDrawResourceBindings(const DrawResourceBinding& binding)
{
    VEX_CHECK(binding.renderTargets.size() <= GMaxSimultaneousRenderTargetCount,
              "Invalid draw resource bindings: Cannot bind more than 8 render targets simultaneously. You attempted to "
              "bind {} render targets.",
              binding.renderTargets.size());

    // Every bound texture and depth-stencil must have the same sliceCount (this is considered as a multi-render target
    // binding).
    std::optional<u32> sliceCount = std::nullopt;
    for (const RenderTargetBinding& rt : binding.renderTargets)
    {
        if (!sliceCount)
        {
            sliceCount = rt.depthSliceCount;
        }
        ValidateRenderTargetBinding(rt);
        VEX_CHECK(*sliceCount == rt.depthSliceCount,
                  "Invalid draw resource bindings: All textures in the binding (Render targets and depth-stencils) "
                  "must have the same slice counts.");
    }

    if (binding.depthStencil)
    {
        u32 depthStencilSliceCount = binding.depthStencil->depthSliceCount;
        if (!sliceCount)
        {
            sliceCount = depthStencilSliceCount;
        }
        ValidateDepthStencilBinding(*binding.depthStencil);
        VEX_CHECK(*sliceCount == depthStencilSliceCount,
                  "Invalid draw resource bindings: All textures in the binding (Render targets and depth-stencils) "
                  "must have the same slice counts.");
    }

    if (binding.indexBuffer)
    {
        ValidateIndexBufferBinding(*binding.indexBuffer);
    }
}

} // namespace BindingUtil

BufferBinding BufferBinding::CreateStructured(const Buffer& buffer,
                                              u32 strideByteSize,
                                              u64 firstElement,
                                              std::optional<u64> elementCount)
{
    VEX_ASSERT(strideByteSize != 0, "Cannot have a stride of 0.");
    const u64 totalElements = buffer.desc.byteSize / strideByteSize;
    const u64 actualElementCount = elementCount.value_or(totalElements - firstElement);
    VEX_ASSERT(firstElement < totalElements,
               "Cannot bind a firstElement ({}) larger than the total elementCount ({}).",
               firstElement,
               totalElements);
    VEX_ASSERT(firstElement + actualElementCount <= totalElements,
               "Cannot bind more elements ({}) than the total elementCount ({}).",
               firstElement + actualElementCount,
               totalElements);
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::StructuredBuffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * static_cast<u64>(strideByteSize),
                .byteSize = actualElementCount * static_cast<u64>(strideByteSize),
            },
        .strideByteSize = strideByteSize,
    };
}

BufferBinding BufferBinding::CreateRWStructured(const Buffer& buffer,
                                                u32 strideByteSize,
                                                u64 firstElement,
                                                std::optional<u64> elementCount)
{
    VEX_ASSERT(strideByteSize != 0, "Cannot have a stride of 0.");
    const u64 totalElements = buffer.desc.byteSize / strideByteSize;
    const u64 actualElementCount = elementCount.value_or(totalElements - firstElement);
    VEX_ASSERT(firstElement < totalElements,
               "Cannot bind a firstElement ({}) larger than the total elementCount ({}).",
               firstElement,
               totalElements);
    VEX_ASSERT(firstElement + actualElementCount <= totalElements,
               "Cannot bind more elements ({}) than the total elementCount ({}).",
               firstElement + actualElementCount,
               totalElements);
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::RWStructuredBuffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * static_cast<u64>(strideByteSize),
                .byteSize = actualElementCount * static_cast<u64>(strideByteSize),
            },
        .strideByteSize = strideByteSize,
    };
}

BufferBinding BufferBinding::CreateRWByteAddress(const Buffer& buffer,
                                                 u64 firstElement,
                                                 std::optional<u64> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::RWByteAddressBuffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * ByteAddressBufferOffsetMultiple,
                .byteSize =
                    elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                    ByteAddressBufferOffsetMultiple,
            },
    };
}

BufferBinding BufferBinding::CreateByteAddress(const Buffer& buffer, u64 firstElement, std::optional<u64> elementCount)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::ByteAddressBuffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * ByteAddressBufferOffsetMultiple,
                .byteSize =
                    elementCount.value_or(buffer.desc.byteSize / ByteAddressBufferOffsetMultiple - firstElement) *
                    ByteAddressBufferOffsetMultiple,
            },
    };
}

BufferBinding BufferBinding::CreateUniform(const Buffer& buffer, u64 offsetByteSize, std::optional<u64> rangeByteSize)
{
    return {
        .buffer = buffer,
        .usage = BufferBindingUsage::UniformBuffer,
        .region =
            BufferRegion{
                .byteOffset = offsetByteSize,
                .byteSize = rangeByteSize.value_or(buffer.desc.byteSize - offsetByteSize),
            },
    };
}

IndexBufferBinding IndexBufferBinding::Create(const Buffer& buffer,
                                              IndexFormat format,
                                              u64 firstElement,
                                              std::optional<u64> elementCount)
{
    return {
        .buffer = buffer,
        .region =
            BufferRegion{
                .byteOffset = firstElement * (std::to_underlying(format) / 8),
                .byteSize = elementCount ? *elementCount * (std::to_underlying(format) / 8) : GBufferWholeSize,
            },
        .format = format,
    };
}

TextureSubresource RenderTargetBinding::GetSubresourceForBarrier() const
{
    return {
        .startMip = mip,
        .mipCount = 1,
        .startSlice = texture.desc.type != TextureType::Texture3D ? startDepthSlice : 0,
        .sliceCount = texture.desc.type != TextureType::Texture3D ? depthSliceCount : 1,
        .aspect = TextureAspect::Color,
    };
}

TextureSubresource DepthStencilBinding::GetSubresourceForBarrier() const
{
    return {
        .startMip = mip,
        .mipCount = 1,
        .startSlice = texture.desc.type != TextureType::Texture3D ? startDepthSlice : 0,
        .sliceCount = texture.desc.type != TextureType::Texture3D ? depthSliceCount : 1,
        .aspect = aspect,
    };
}

} // namespace vex
