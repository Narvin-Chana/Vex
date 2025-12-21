#pragma once

#include <string>

#include <Vex/Utility/EnumFlags.h>
#include <Vex/Formats.h>
#include <Vex/Utility/Handle.h>
#include <Vex/Utility/Hash.h>
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

struct TextureDesc
{
    std::string name;
    TextureType type;
    TextureFormat format;
    u32 width = 1, height = 1, depthOrSliceCount = 1;
    u16 mips = 1;
    TextureUsage::Flags usage = TextureUsage::ShaderRead;
    TextureClearValue clearValue;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;

    [[nodiscard]] u32 GetDepth() const
    {
        return type == TextureType::Texture3D ? depthOrSliceCount : 1;
    }
    [[nodiscard]] u32 GetSliceCount() const
    {
        if (type == TextureType::Texture3D)
        {
            return 1;
        }

        u32 sliceCount = depthOrSliceCount;
        if (type == TextureType::TextureCube)
        {
            // Cubemaps are just a Texture2DArray with an array size which is a multiple of 6.
            sliceCount *= GTextureCubeFaceCount;
        }
        return sliceCount;
    }

    // Helpers to create a texture description.

    static TextureDesc CreateTexture2DDesc(std::string name,
                                           TextureFormat format,
                                           u32 width,
                                           u32 height,
                                           u16 mips = 1,
                                           TextureUsage::Flags usage = TextureUsage::ShaderRead,
                                           TextureClearValue clearValue = {},
                                           ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDesc CreateTexture2DArrayDesc(
        std::string name,
        TextureFormat format,
        u32 width,
        u32 height,
        u32 arraySize,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDesc CreateTextureCubeDesc(std::string name,
                                             TextureFormat format,
                                             u32 faceSize,
                                             u16 mips = 1,
                                             TextureUsage::Flags usage = TextureUsage::ShaderRead,
                                             TextureClearValue clearValue = {},
                                             ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDesc CreateTextureCubeArrayDesc(
        std::string name,
        TextureFormat format,
        u32 faceSize,
        u32 arraySize,
        u16 mips = 1,
        TextureUsage::Flags usage = TextureUsage::ShaderRead,
        TextureClearValue clearValue = {},
        ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly);

    static TextureDesc CreateTexture3DDesc(std::string name,
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
struct TextureHandle : public Handle64<TextureHandle>
{
};

static constexpr TextureHandle GInvalidTextureHandle;

struct Texture final
{
    TextureHandle handle;
    TextureDesc desc;
};

static constexpr u32 GTextureExtentMax = ~static_cast<u32>(0);

struct TextureExtent3D
{
    u32 width = GTextureExtentMax;
    u32 height = GTextureExtentMax;
    u32 depth = GTextureExtentMax;

    u32 GetWidth(const TextureDesc& desc, u16 mipIndex) const;
    u32 GetHeight(const TextureDesc& desc, u16 mipIndex) const;
    u32 GetDepth(const TextureDesc& desc, u16 mipIndex) const;

    constexpr bool operator==(const TextureExtent3D&) const = default;
};

struct TextureOffset3D
{
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;

    constexpr bool operator==(const TextureOffset3D&) const = default;
};

static constexpr u16 GTextureAllMips = ~static_cast<u16>(0);
static constexpr u32 GTextureAllSlices = ~static_cast<u32>(0);

static constexpr u32 GTextureClearRectMax = ~static_cast<u32>(0);

struct TextureClearRect
{
    i32 offsetX = 0, offsetY = 0;
    u32 extentX = GTextureClearRectMax, extentY = GTextureClearRectMax;

    u32 GetExtentX(const TextureDesc& desc) const;
    u32 GetExtentY(const TextureDesc& desc) const;

    constexpr bool operator==(const TextureClearRect&) const = default;
};

struct TextureSubresource
{
    u16 startMip = 0;
    u16 mipCount = GTextureAllMips;
    u32 startSlice = 0;
    u32 sliceCount = GTextureAllSlices;

    u16 GetMipCount(const TextureDesc& desc) const;
    u32 GetSliceCount(const TextureDesc& desc) const;

    constexpr bool operator==(const TextureSubresource&) const = default;
};

struct TextureRegion
{
    TextureSubresource subresource;
    TextureOffset3D offset;
    TextureExtent3D extent;

    std::tuple<u32, u32, u32> GetExtents(const TextureDesc& desc, u16 mip) const;

    constexpr bool operator==(const TextureRegion&) const = default;

    // The entirety of the texture (all mips and all slices).
    static TextureRegion AllMips();
    // The entirety of a single mip (one mip and all slices).
    static TextureRegion SingleMip(u16 mipIndex);
};

struct TextureCopyDesc
{
    TextureRegion srcRegion;
    TextureRegion dstRegion;

    constexpr bool operator==(const TextureCopyDesc&) const = default;
};

struct TextureBinding;

namespace TextureUtil
{

static constexpr u64 RowPitchAlignment = 256;
static constexpr u64 MipAlignment = 512;

std::tuple<u32, u32, u32> GetMipSize(const TextureDesc& desc, u32 mip);
TextureViewType GetTextureViewType(const TextureBinding& binding);
void ValidateTextureDescription(const TextureDesc& desc);
float GetPixelByteSizeFromFormat(TextureFormat format);

u64 ComputeAlignedUploadBufferByteSize(const TextureDesc& desc, Span<const TextureRegion> uploadRegions);
u64 ComputePackedTextureDataByteSize(const TextureDesc& desc, Span<const TextureRegion> uploadRegions);

bool IsBindingUsageCompatibleWithUsage(TextureUsage::Flags usages, TextureBindingUsage bindingUsage);

void ValidateSubresource(const TextureDesc& desc, const TextureSubresource& subresource);
void ValidateRegion(const TextureDesc& desc, const TextureRegion& region);
void ValidateCopyDesc(const TextureDesc& srcDesc, const TextureDesc& dstDesc, const TextureCopyDesc& copyDesc);
void ValidateCompatibleTextureDescs(const TextureDesc& srcDesc, const TextureDesc& dstDesc);

} // namespace TextureUtil

} // namespace vex

// clang-format off
VEX_MAKE_HASHABLE(vex::TextureSubresource,
    VEX_HASH_COMBINE(seed, obj.startMip);
    VEX_HASH_COMBINE(seed, obj.mipCount);
    VEX_HASH_COMBINE(seed, obj.startSlice);
    VEX_HASH_COMBINE(seed, obj.sliceCount);
);
// clang-format on
