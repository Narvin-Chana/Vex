#pragma once

#include <limits>
#include <string>

#include <Vex/Formats.h>
#include <Vex/Handle.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

namespace vex
{

enum class TextureType
{
    Texture2D,
    TextureCube,
    Texture3D,
};

// Used internally for views (eg: a cubemap can either be interpreted as a 6 slice Texture2DArray or a TextureCube).
enum class TextureViewType
{
    Texture2D,
    Texture2DArray,
    TextureCube,
    TextureCubeArray,
    Texture3D,
};

struct ResourceBinding;

namespace TextureUtil
{
TextureViewType GetTextureViewType(const ResourceBinding& binding);
TextureFormat GetTextureFormat(const ResourceBinding& binding);
} // namespace TextureUtil

struct TextureClearValue
{
    bool enabled = false;
    float color[4];
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
    ResourceUsage::Flags usage = ResourceUsage::Read;
    TextureClearValue clearValue;
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

} // namespace vex