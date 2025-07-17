#include "Bindings.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

namespace vex
{

void ResourceBinding::ValidateResourceBindings(std::span<const ResourceBinding> bindings,
                                               ResourceUsage::Flags validUsageFlags)
{
    bool depthStencilAlreadyFound = false;

    for (const auto& resource : bindings)
    {
        if (resource.name.empty())
        {
            VEX_LOG(Fatal, "Invalid binding: You must specify a non-empty name.");
        }

        if (!resource.IsBuffer() && !resource.IsTexture())
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": You must specify either a buffer or texture.",
                    resource.name);
        }

        if (!(resource.texture.description.usage & validUsageFlags))
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
                    "operation. Check the usage flags of your resource at creation.",
                    resource.name);
        }

        if (resource.IsTexture())
        {
            if (resource.mipCount > resource.texture.description.mips)
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": The binding's mip count ({}) cannot be larger than the "
                        "actual texture's mip count ({}).",
                        resource.name,
                        resource.mipCount,
                        resource.texture.description.mips);
            }

            if (resource.mipBias >= resource.texture.description.mips)
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": The binding's mip bias ({}) cannot be larger than the "
                        "actual texture's mip count ({}).",
                        resource.name,
                        resource.mipBias,
                        resource.texture.description.mips);
            }

            if (resource.texture.description.depthOrArraySize > 1 &&
                resource.sliceCount > resource.texture.description.depthOrArraySize)
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": The binding's slice count ({}) cannot be larger than the "
                        "actual texture's depth ({}).",
                        resource.name,
                        resource.sliceCount,
                        resource.texture.description.depthOrArraySize);
            }

            if (resource.texture.description.depthOrArraySize > 1 &&
                resource.startSlice >= resource.texture.description.depthOrArraySize)
            {
                VEX_LOG(
                    Fatal,
                    "Invalid binding for resource \"{}\": The binding's starting slice ({}) cannot be larger than the "
                    "actual texture's depth ({}).",
                    resource.name,
                    resource.startSlice,
                    resource.texture.description.depthOrArraySize);
            }

            if (resource.textureFlags & TextureBinding::SRGB)
            {
                if (!FormatHasSRGBEquivalent(resource.texture.description.format))
                {
                    VEX_LOG(Fatal,
                            "Invalid binding for resource \"{}\": Texture's format ({}) does not allow for an SRGB "
                            "binding.",
                            resource.name,
                            magic_enum::enum_name(resource.texture.description.format));
                }
            }

            if (FormatIsDepthStencilCompatible(resource.texture.description.format) &&
                !(resource.texture.description.usage & ResourceUsage::DepthStencil))
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                        "upon creation.",
                        resource.name,
                        magic_enum::enum_name(resource.texture.description.format));
            }

            if (FormatIsDepthStencilCompatible(resource.texture.description.format) &&
                (validUsageFlags & ResourceUsage::DepthStencil))
            {
                if (depthStencilAlreadyFound)
                {
                    VEX_LOG(Fatal,
                            "Invalid binding for resource \"{}\": Cannot bind multiple depth stencils to the graphics "
                            "pipeline.",
                            resource.name);
                }
                depthStencilAlreadyFound = true;
            }
        }

        if (resource.IsBuffer())
        {
            if (!((validUsageFlags & ResourceUsage::Read) || (validUsageFlags & ResourceUsage::UnorderedAccess)))
            {
                VEX_LOG(
                    Fatal,
                    "Invalid binding for resource \"{}\": A buffer cannot be bound as a render target/depth stencil.",
                    resource.name);
            }

            // TODO: implement validation for buffers.
            VEX_NOT_YET_IMPLEMENTED();
        }
    }
}

} // namespace vex
