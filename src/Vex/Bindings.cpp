#include "Bindings.h"

#include <numeric>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

namespace vex
{

void BufferBinding::ValidateForShaderUse(BufferUsage::Flags validBufferUsageFlags) const
{
    Validate();

    if (!(buffer.description.usage & validBufferUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                buffer.description.name);
    }
}

void BufferBinding::Validate() const
{
    if (!IsBindingUsageCompatibleWithBufferUsage(buffer.description.usage, usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
                "usage.",
                buffer.description.name);
    }

    if (usage == BufferBindingUsage::Invalid)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                buffer.description.name);
    }

    if (usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::RWStructuredBuffer)
    {
        if (!stride.has_value())
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": In order to use a binding as a structured buffer, you must "
                    "pass in a valid stride.",
                    buffer.description.name);
        }
    }
}

void TextureBinding::ValidateForShaderUse(TextureUsage::Flags validTextureUsageFlags) const
{
    Validate();

    if (!(texture.description.usage & validTextureUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                texture.description.name);
    }

    if ((validTextureUsageFlags & TextureUsage::DepthStencil) &&
        !FormatIsDepthStencilCompatible(texture.description.format))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": texture cannot be bound as depth stencil",
                texture.description.name);
    }
}
void TextureBinding::Validate() const
{
    if (usage == TextureBindingUsage::None)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                texture.description.name);
    }

    if (mipCount > texture.description.mips)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's mip count ({}) cannot be larger than the "
                "actual texture's mip count ({}).",
                texture.description.name,
                mipCount,
                texture.description.mips);
    }

    if (mipBias >= texture.description.mips)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's mip bias ({}) cannot be larger than the "
                "actual texture's mip count ({}).",
                texture.description.name,
                mipBias,
                texture.description.mips);
    }

    if (texture.description.depthOrArraySize > 1 && sliceCount > texture.description.depthOrArraySize)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's slice count ({}) cannot be larger than the "
                "actual texture's depth ({}).",
                texture.description.name,
                sliceCount,
                texture.description.depthOrArraySize);
    }

    if (texture.description.depthOrArraySize > 1 && startSlice >= texture.description.depthOrArraySize)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's starting slice ({}) cannot be larger than the "
                "actual texture's depth ({}).",
                texture.description.name,
                startSlice,
                texture.description.depthOrArraySize);
    }

    if (flags & TextureBindingFlags::SRGB)
    {
        if (!FormatHasSRGBEquivalent(texture.description.format))
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": Texture's format ({}) does not allow for an SRGB "
                    "binding.",
                    texture.description.name,
                    magic_enum::enum_name(texture.description.format));
        }
    }

    if (FormatIsDepthStencilCompatible(texture.description.format) &&
        !(texture.description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                "upon creation.",
                texture.description.name,
                magic_enum::enum_name(texture.description.format));
    }

    if (!IsTextureBindingUsageCompatibleWithTextureUsage(texture.description.usage, usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with texture description's"
                "usage.",
                texture.description.name);
    }
}

void DrawResourceBinding::Validate() const
{
    for (const auto& binding : renderTargets)
    {
        binding.ValidateForShaderUse(TextureUsage::RenderTarget);
    }

    if (depthStencil)
    {
        depthStencil->ValidateForShaderUse(TextureUsage::DepthStencil);
    }
}

} // namespace vex
