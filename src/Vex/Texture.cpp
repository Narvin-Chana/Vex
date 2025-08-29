#include "Bindings.h"
#include "Logger.h"
#include "Texture.h"

namespace vex
{

namespace TextureUtil
{

TextureViewType GetTextureViewType(const TextureBinding& binding)
{
    switch (binding.texture.description.type)
    {
    case TextureType::Texture2D:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::Texture2DArray
                                                                  : TextureViewType::Texture2D;
    case TextureType::TextureCube:
        return (binding.texture.description.depthOrArraySize > 1) ? TextureViewType::TextureCubeArray
                                                                  : TextureViewType::TextureCube;
    case TextureType::Texture3D:
        return TextureViewType::Texture3D;
    default:
        VEX_LOG(Fatal, "Unrecognized texture type...");
    }
    std::unreachable();
}

TextureFormat GetTextureFormat(const TextureBinding& binding)
{
    if (!IsFormatSRGB(binding.texture.description.format) &&
        !FormatHasSRGBEquivalent(binding.texture.description.format))
    {
        return TextureFormat::UNKNOWN;
    }

    if (binding.flags & TextureBindingFlags::SRGB)
    {
        return GetSRGBEquivalentFormat(binding.texture.description.format);
    }

    return binding.texture.description.format;
}

void ValidateTextureDescription(const TextureDescription& description)
{
    bool isDepthStencilFormat = FormatIsDepthStencilCompatible(description.format);
    if (isDepthStencilFormat && !(description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a DepthStencil usage must have a "
                "format that supports it",
                description.name);
    }

    if (!isDepthStencilFormat && (description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture that has a non-depth-stencil compatible "
                "format must allow for DepthStencil usage.",
                description.name);
    }

    if ((description.usage & TextureUsage::RenderTarget) && (description.usage & TextureUsage::DepthStencil))
    {
        VEX_LOG(Fatal,
                "Invalid Texture description for texture \"{}\": A texture cannot have both RenderTarget AND "
                "DepthStencil usage.",
                description.name);
    }
}

} // namespace TextureUtil

u32 TextureDescription::GetTextureByteSize() const
{
    // TODO: Split this per API as the memory footprint of textures varies
    // This is unused for now. To be tackled properly when uploading data to texture
    return width * height * depthOrArraySize * (4 * sizeof(float));
}
} // namespace vex
