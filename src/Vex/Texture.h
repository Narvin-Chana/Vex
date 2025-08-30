#pragma once

#include <regex>
#include <string>

#include <Vex/EnumFlags.h>
#include <Vex/Formats.h>
#include <Vex/Handle.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

namespace vex
{
// clang-format off

BEGIN_VEX_ENUM_FLAGS(TextureUsage, u8)
    None            = 0,
    ShaderRead      = 1 << 0, // SRV in DX12, Sampled/Combined Image in Vulkan
    ShaderReadWrite = 1 << 1, // UAV in DX12, Storage Image in Vulkan
    RenderTarget    = 1 << 2, // RTV in DX12, Color Attachment in Vulkan
    DepthStencil    = 1 << 3, // DSV in DX12, Depth/Stencil Attachment in Vulkan
END_VEX_ENUM_FLAGS();

//clang-format on

enum class TextureType : u8
{
    Texture2D,
    TextureCube,
    Texture3D,
};

// Used internally for views (eg: a cubemap can either be interpreted as a 6 slice Texture2DArray or a TextureCube).
enum class TextureViewType : u8
{
    Texture2D,
    Texture2DArray,
    TextureCube,
    TextureCubeArray,
    Texture3D,
};

enum class TextureBindingUsage : u8
{
    None = 0,
    ShaderRead = TextureUsage::ShaderRead,
    ShaderReadWrite = TextureUsage::ShaderReadWrite,
};

struct ResourceBinding;
struct TextureBinding;
struct TextureDescription;

namespace TextureUtil
{

TextureViewType GetTextureViewType(const TextureBinding& binding);
TextureFormat GetTextureFormat(const TextureBinding& binding);

void ValidateTextureDescription(const TextureDescription& description)
;
} // namespace TextureUtil

// clang-format off

BEGIN_VEX_ENUM_FLAGS(TextureClear, u8)
    None = 0,
    ClearColor = 1,
    ClearDepth = 2,
    ClearStencil = 4,
END_VEX_ENUM_FLAGS();

// clang-format on

struct TextureClearValue
{
    TextureClear::Flags flags = TextureClear::None;
    std::array<float, 4> color;
    float depth;
    u8 stencil;
};

struct TextureDescription
{
    std::string name;
    TextureType type;
    u32 width, height, depthOrArraySize = 1;
    // mips = 0 indicates that you want max mips
    u16 mips = 1;
    TextureFormat format;
    TextureUsage::Flags usage = TextureUsage::ShaderRead;
    TextureClearValue clearValue;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;

    u32 GetTextureByteSize() const;
};

// Strongly defined type represents a texture.
// We use a struct (instead of a typedef/using) to enforce compile-time correctness of handles.
struct TextureHandle : public Handle<TextureHandle>
{
};

static constexpr TextureHandle GInvalidTextureHandle;

struct Texture final
{
    TextureHandle handle;
    TextureDescription description;
};

inline u8 GetPixelByteSizeFromFormat(TextureFormat format)
{
    const std::string_view enumName = magic_enum::enum_name(format);

    const std::regex r{ "([0-9]+)+" };

    std::match_results<std::string_view::const_iterator> results;

    auto it = enumName.begin();

    int totalBits = 0;
    while (regex_search(it, enumName.end(), results, r))
    {
        std::string value = results.str();
        totalBits += std::stoi(value);
        std::advance(it, results.prefix().length() + value.length());
    }

    return static_cast<uint8_t>(totalBits / 8);
}

inline bool IsTextureBindingUsageCompatibleWithTextureUsage(TextureUsage::Flags usages,
                                                            TextureBindingUsage bindingUsage)
{
    if (bindingUsage == TextureBindingUsage::ShaderRead)
    {
        return usages & TextureUsage::ShaderRead;
    }

    if (bindingUsage == TextureBindingUsage::ShaderReadWrite)
    {
        return usages & TextureUsage::ShaderReadWrite;
    }

    return true;
}

struct TextureRegionExtent
{
    u32 width;
    u32 height;
    u32 depth = 1;
};

struct TextureRegion
{
    u32 mip = 0;
    u32 baseLayer = 0;
    u32 layerCount = 1;
    TextureRegionExtent offset{ 0, 0, 0 };
};

struct TextureToTextureCopyRegionMapping
{
    TextureRegion srcRegion;
    TextureRegion dstRegion;
    TextureRegionExtent extent;
};

} // namespace vex