#include "Bindings.h"

#include <numeric>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

namespace vex
{

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
        if (!binding.strideByteSize.has_value())
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": In order to use a binding as a structured buffer, you must "
                    "pass in a valid stride.",
                    buffer.desc.name);
        }
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

    if ((validTextureUsageFlags & TextureUsage::DepthStencil) && !FormatIsDepthStencilCompatible(texture.desc.format))
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

    TextureUtil::ValidateSubresource(binding.subresource, texture.desc);

    if (binding.flags & TextureBindingFlags::SRGB)
    {
        if (!FormatHasSRGBEquivalent(texture.desc.format))
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": Texture's format ({}) does not allow for an SRGB "
                    "binding.",
                    texture.desc.name,
                    magic_enum::enum_name(texture.desc.format));
        }
    }

    if (FormatIsDepthStencilCompatible(texture.desc.format) && !(texture.desc.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                "upon creation.",
                texture.desc.name,
                magic_enum::enum_name(texture.desc.format));
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

} // namespace vex
