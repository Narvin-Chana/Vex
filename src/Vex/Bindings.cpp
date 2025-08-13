#include "Bindings.h"

#include <numeric>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

namespace vex
{

namespace Bindings_Internal
{
static bool IsValidHLSLResourceName(const std::string& name)
{
    // Check if empty
    if (name.empty())
    {
        return false;
    }

    // First character must be a letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_')
    {
        return false;
    }

    // Check each character
    for (char c : name)
    {
        // Valid characters: letters, digits, underscore
        if (!std::isalnum(c) && c != '_')
        {
            return false;
        }
    }

    return true;
}
} // namespace Bindings_Internal

u32 ConstantBinding::ConcatConstantBindings(std::span<const ConstantBinding> constantBindings,
                                            std::span<u8> writableRange)
{
    const u32 total = std::accumulate(constantBindings.begin(),
                                      constantBindings.end(),
                                      0u,
                                      [](u32 acc, const ConstantBinding& binding) { return acc + binding.size; });

    if (total > writableRange.size())
    {
        VEX_LOG(Fatal,
                "Unable to create local constants buffer, you have surpassed the limit allowed for local constants.");
    }

    u8 currentIndex = 0;
    for (const auto& binding : constantBindings)
    {
        std::uninitialized_copy_n(static_cast<const u8*>(binding.data), binding.size, &writableRange[currentIndex]);
        currentIndex += binding.size;
    }

    return total;
}

void BufferBinding::ValidateForUse(BufferUsage::Flags validBufferUsageFlags) const
{
    Validate();

    if (!(buffer.description.usage & validBufferUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                name);
    }
}

void BufferBinding::Validate() const
{
    if (!Bindings_Internal::IsValidHLSLResourceName(name))
    {
        VEX_LOG(Fatal,
                "Invalid binding for buffer \"{}\": You must specify a non-empty name that is valid for HLSL usage.",
                buffer.description.name);
    }

    if (usage == BufferBindingUsage::Invalid)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                name);
    }

    if (!IsBindingUsageCompatibleWithBufferUsage(buffer.description.usage, usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
                "usage.",
                name);
    }
}

void TextureBinding::ValidateForUse(TextureUsage::Flags validTextureUsageFlags) const
{
    Validate();

    if (!(texture.description.usage & validTextureUsageFlags))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
                "operation. Check the usage flags of your resource at creation.",
                name);
    }

    if ((validTextureUsageFlags & TextureUsage::DepthStencil) &&
        !FormatIsDepthStencilCompatible(texture.description.format))
    {
        VEX_LOG(Fatal, "Invalid binding for resource \"{}\": texture cannot be bound as depth stencil", name);
    }
}
void TextureBinding::Validate() const
{
    if (!Bindings_Internal::IsValidHLSLResourceName(name))
    {
        VEX_LOG(Fatal,
                "Invalid binding for texture \"{}\": You must specify a non-empty name that is valid for HLSL usage.",
                texture.description.name);
    }

    if (usage == TextureBindingUsage::Invalid)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's usage must be set to something and therefore not "
                "be invalid",
                name);
    }

    if (mipCount > texture.description.mips)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's mip count ({}) cannot be larger than the "
                "actual texture's mip count ({}).",
                name,
                mipCount,
                texture.description.mips);
    }

    if (mipBias >= texture.description.mips)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's mip bias ({}) cannot be larger than the "
                "actual texture's mip count ({}).",
                name,
                mipBias,
                texture.description.mips);
    }

    if (texture.description.depthOrArraySize > 1 && sliceCount > texture.description.depthOrArraySize)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's slice count ({}) cannot be larger than the "
                "actual texture's depth ({}).",
                name,
                sliceCount,
                texture.description.depthOrArraySize);
    }

    if (texture.description.depthOrArraySize > 1 && startSlice >= texture.description.depthOrArraySize)
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": The binding's starting slice ({}) cannot be larger than the "
                "actual texture's depth ({}).",
                name,
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
                    name,
                    magic_enum::enum_name(texture.description.format));
        }
    }

    if (FormatIsDepthStencilCompatible(texture.description.format) &&
        !(texture.description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                "upon creation.",
                name,
                magic_enum::enum_name(texture.description.format));
    }

    if (!IsTextureBindingUsageCompatibleWithTextureUsage(texture.description.usage, usage))
    {
        VEX_LOG(Fatal,
                "Invalid binding for resource \"{}\": Binding usage must be compatible with texture description's"
                "usage.",
                name);
    }
}

void DrawResourceBinding::Validate() const
{
    for (const auto& binding : renderTargets)
    {
        binding.ValidateForUse(TextureUsage::RenderTarget);
    }

    if (depthStencil)
    {
        depthStencil->ValidateForUse(TextureUsage::DepthStencil);
    }
}

} // namespace vex
