#pragma once

#include <limits>
#include <string>

#include <Vex/Formats.h>
#include <Vex/Handle.h>
#include <Vex/Types.h>

namespace vex
{

enum class TextureType
{
    Texture2D,
    TextureCube,
    Texture3D,
};

struct TextureDescription
{
    std::string name;
    TextureType type;
    u32 width, height, depthOrArraySize;
    u16 mips;
    TextureFormat format;
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