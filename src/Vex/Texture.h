#pragma once

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

// clang-format on

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
    ShaderRead = TextureUsage::ShaderRead,           // Equivalent to SRV in DX12.
    ShaderReadWrite = TextureUsage::ShaderReadWrite, // Equivalent to UAV in DX12.
};

struct ResourceBinding;
struct TextureBinding;
struct TextureDescription;

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

static constexpr u8 GTextureCubeFaceCount = 6U;

struct TextureDescription
{
    std::string name;
    TextureType type;
    TextureFormat format;
    u32 width, height, depthOrArraySize = 1;
    // mips = 0 indicates that you want max mips
    u16 mips = 1;
    TextureUsage::Flags usage = TextureUsage::ShaderRead;
    TextureClearValue clearValue;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;

    [[nodiscard]] u32 GetDepth() const noexcept
    {
        return type == TextureType::Texture3D ? depthOrArraySize : 1;
    }
    [[nodiscard]] u32 GetArraySize() const noexcept
    {
        if (type == TextureType::Texture3D)
        {
            return 1;
        }

        u32 arraySize = depthOrArraySize;
        if (type == TextureType::TextureCube)
        {
            // Cubemaps are just a Texture2DArray with an array size which is a multiple of 6.
            arraySize *= GTextureCubeFaceCount;
        }
        return arraySize;
    }

    // Helpers to create a texture description.

    static TextureDescription CreateTexture2DDesc(
        std::string name,
        TextureFormat format,
        u32 width,
        u32 height,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDescription CreateTexture2DArrayDesc(
        std::string name,
        TextureFormat format,
        u32 width,
        u32 height,
        u32 arraySize,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDescription CreateTextureCubeDesc(
        std::string name,
        TextureFormat format,
        u32 faceSize,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDescription CreateTextureCubeArrayDesc(
        std::string name,
        TextureFormat format,
        u32 faceSize,
        u32 arraySize,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDescription CreateTexture3DDesc(
        std::string name,
        TextureFormat format,
        u32 width,
        u32 height,
        u32 depth,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);
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

struct TextureExtent
{
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1;
};

struct TextureSubresource
{
    // Mip index (0-based).
    u16 mip = 0;
    u32 startSlice = 0;
    u32 sliceCount = 1;
    TextureExtent offset{ 0, 0, 0 };
};

struct TextureCopyDescription
{
    TextureSubresource srcSubresource;
    TextureSubresource dstSubresource;
    TextureExtent extent;
};

struct TextureUploadRegion
{
    u16 mip = 0;
    u32 slice = 0;
    TextureExtent offset{ 0, 0, 0 };
    TextureExtent extent{ 0, 0, 0 };

    // Uploads the entirety of the texture (all mips and all depth slices).
    static std::vector<TextureUploadRegion> UploadAllMips(const TextureDescription& textureDescription);
    // Uploads the entirety of a specific mip of the texture (1 mip and all depth slices).
    static std::vector<TextureUploadRegion> UploadFullMip(u16 mipIndex, const TextureDescription& textureDescription);
};

namespace TextureUtil
{

static constexpr u64 RowPitchAlignment = 256;
static constexpr u64 MipAlignment = 512;

std::tuple<u32, u32, u32> GetMipSize(const TextureDescription& desc, u32 mip);
TextureViewType GetTextureViewType(const TextureBinding& binding);
TextureFormat GetTextureFormat(const TextureBinding& binding);
void ValidateTextureDescription(const TextureDescription& description);
float GetPixelByteSizeFromFormat(TextureFormat format);

u64 ComputeAlignedUploadBufferByteSize(const TextureDescription& desc,
                                       std::span<const TextureUploadRegion> uploadRegions);
u64 ComputePackedUploadDataByteSize(const TextureDescription& desc, std::span<const TextureUploadRegion> uploadRegions);

bool IsTextureBindingUsageCompatibleWithTextureUsage(TextureUsage::Flags usages, TextureBindingUsage bindingUsage);

void ValidateTextureSubresource(const TextureDescription& description, const TextureSubresource& subresource);
void ValidateTextureCopyDescription(const TextureDescription& srcDesc,
                                    const TextureDescription& dstDesc,
                                    const TextureCopyDescription& copyDesc);
void ValidateTextureExtent(const TextureDescription& description,
                           const TextureSubresource& subresource,
                           const TextureExtent& extent);
void ValidateCompatibleTextureDescriptions(const TextureDescription& srcDesc, const TextureDescription& dstDesc);

} // namespace TextureUtil

} // namespace vex