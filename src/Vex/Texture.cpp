#include "Texture.h"

#include "Bindings.h"
#include "Logger.h"

namespace vex
{

namespace TextureUtil
{

TextureViewType GetTextureViewType(const ResourceBinding& binding)
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

TextureFormat GetTextureFormat(const ResourceBinding& binding)
{
    if (!IsFormatSRGB(binding.texture.description.format) &&
        !FormatHasSRGBEquivalent(binding.texture.description.format))
    {
        return TextureFormat::UNKNOWN;
    }

    if (binding.textureFlags & TextureBinding::SRGB)
    {
        return GetSRGEquivalentFormat(binding.texture.description.format);
    }

    return binding.texture.description.format;
}

} // namespace TextureUtil

} // namespace vex
