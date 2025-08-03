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

std::vector<u8> ConstantBinding::ConcatConstantBindings(std::span<const ConstantBinding> constantBindings,
                                                        u32 maxBufferSize)
{
    const u32 total = std::accumulate(constantBindings.begin(),
                                      constantBindings.end(),
                                      0u,
                                      [](u32 acc, const ConstantBinding& binding) { return acc + binding.size; });

    if (total <= maxBufferSize)
    {
        VEX_LOG(Fatal,
                "Unable to create local constants buffer, you have surpassed the limit allowed for local constants.");
    }

    std::vector<u8> constantDataBuffer;
    constantDataBuffer.resize(total);

    u8 currentIndex = 0;
    for (const auto& binding : constantBindings)
    {
        std::uninitialized_copy_n(static_cast<const u8*>(binding.data),
                                  binding.size,
                                  &constantDataBuffer[currentIndex]);
        currentIndex += binding.size;
    }

    return constantDataBuffer;
}

void ResourceBinding::ValidateResourceBindings(std::span<const ResourceBinding> bindings,
                                               TextureUsage::Flags validTextureUsageFlags,
                                               BufferUsage::Flags validBufferUsageFlags)
{
    bool depthStencilAlreadyFound = false;

    for (const auto& resource : bindings)
    {
        if (!Bindings_Internal::IsValidHLSLResourceName(resource.name))
        {
            VEX_LOG(Fatal, "Invalid binding: You must specify a non-empty name that is valid for HLSL usage.");
        }

        if (!resource.IsBuffer() && !resource.IsTexture())
        {
            VEX_LOG(Fatal,
                    "Invalid binding for resource \"{}\": You must specify either a buffer or texture.",
                    resource.name);
        }

        if (resource.IsTexture())
        {
            if (!(resource.texture.description.usage & validTextureUsageFlags))
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": The specified texture cannot be bound for this type of "
                        "operation. Check the usage flags of your resource at creation.",
                        resource.name);
            }

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
                !(resource.texture.description.usage & TextureUsage::DepthStencil))
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": Texture's format ({}) requires the depth stencil usage "
                        "upon creation.",
                        resource.name,
                        magic_enum::enum_name(resource.texture.description.format));
            }

            if (FormatIsDepthStencilCompatible(resource.texture.description.format) &&
                (validTextureUsageFlags & TextureUsage::DepthStencil))
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

            if (resource.bufferUsage != BufferBindingUsage::None)
            {
                VEX_LOG(
                    Warning,
                    "Invalid binding for resource \"{}\": buffer binding flags is set. These are ignored for textures.",
                    resource.name);
            }
        }

        if (resource.IsBuffer())
        {
            if (!(resource.buffer.description.usage & validBufferUsageFlags))
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": The specified buffer cannot be bound for this type of "
                        "operation. Check the usage flags of your resource at creation.",
                        resource.name);
            }

            if (!IsBindingUsageCompatibleWithBufferUsage(resource.buffer.description.usage, resource.bufferUsage))
            {
                VEX_LOG(Fatal,
                        "Invalid binding for resource \"{}\": Binding usage must be compatible with buffer description "
                        "usage.",
                        resource.name);
            }

            if (resource.mipBias != 0)
            {
                VEX_LOG(
                    Warning,
                    "Invalid binding for resource \"{}\": mipBias is set to {}. This parameter is ignored for buffers.",
                    resource.name,
                    resource.mipBias);
            }

            if (resource.mipCount != 0)
            {
                VEX_LOG(Warning,
                        "Invalid binding for resource \"{}\": mipCount is set to {}. This parameter is ignored for "
                        "buffers.",
                        resource.name,
                        resource.mipCount);
            }

            if (resource.startSlice != 0)
            {
                VEX_LOG(Warning,
                        "Invalid binding for resource \"{}\": startSlice is set to {}. This parameter is ignored for "
                        "buffers.",
                        resource.name,
                        resource.startSlice);
            }

            if (resource.sliceCount != 0)
            {
                VEX_LOG(Warning,
                        "Invalid binding for resource \"{}\": sliceCount is set to {}. This parameter is ignored for "
                        "buffers.",
                        resource.name,
                        resource.sliceCount);
            }

            if (resource.textureFlags != TextureBinding::None)
            {
                VEX_LOG(
                    Warning,
                    "Invalid binding for resource \"{}\": texture binding flags is set. These are ignored for buffers.",
                    resource.name);
            }
        }
    }
}

} // namespace vex
