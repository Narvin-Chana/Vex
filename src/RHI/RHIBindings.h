#pragma once

#include <optional>

#include <Vex/Buffer.h>
#include <Vex/Containers/InlineVector.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Texture.h>
#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct TextureViewDesc
{
    TextureViewType viewType;
    TextureFormat format;
    bool isSRGB = false;
    TextureUsage usage = TextureUsage::None;
    TextureSubresource subresource;

    constexpr bool operator==(const TextureViewDesc&) const = default;
};

struct BufferViewDesc
{
    BufferBindingUsage usage = BufferBindingUsage::Invalid;
    BufferRegion region;
    u32 strideByteSize = 0;
    bool isAccelerationStructure = false;

    constexpr bool operator==(const BufferViewDesc&) const = default;

    u32 GetElementStride() const
    {
        switch (usage)
        {
        case BufferBindingUsage::StructuredBuffer:
        case BufferBindingUsage::RWStructuredBuffer:
            return strideByteSize;
        case BufferBindingUsage::ByteAddressBuffer:
        case BufferBindingUsage::RWByteAddressBuffer:
            return 4;
        default:
            break;
        }
        return strideByteSize;
    }

    u64 GetFirstElement() const
    {
        if (usage == BufferBindingUsage::UniformBuffer)
        {
            return 0;
        }

        return region.byteOffset / GetElementStride();
    }

    u64 GetElementCount(const BufferDesc& desc) const
    {
        if (usage == BufferBindingUsage::UniformBuffer)
        {
            return 1;
        }

        return region.GetByteSize(desc) / GetElementStride();
    }
};

// clang-format off
struct RHIRenderTargetView  { NonNullPtr<RHITexture> texture; TextureViewDesc view; };
struct RHIDepthStencilView  { NonNullPtr<RHITexture> texture; TextureViewDesc view; };
struct RHITextureView       { NonNullPtr<RHITexture> texture; TextureViewDesc view; };
struct RHIBufferView        { NonNullPtr<RHIBuffer> buffer; BufferViewDesc view; };
struct RHIIndexBufferView   { NonNullPtr<RHIBuffer> buffer; BufferRegion region; IndexFormat format; };
// clang-format on

struct RHIDrawResources
{
    InlineVector<RHIRenderTargetView, GMaxSimultaneousRenderTargetCount> renderTargets;
    std::optional<RHIDepthStencilView> depthStencil = std::nullopt;
};

} // namespace vex

// clang-format off
VEX_MAKE_HASHABLE(vex::TextureViewDesc,
    VEX_HASH_COMBINE(seed, obj.viewType);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.isSRGB);
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.subresource);
);

VEX_MAKE_HASHABLE(vex::BufferViewDesc,
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.region);
    VEX_HASH_COMBINE(seed, obj.strideByteSize);
    VEX_HASH_COMBINE(seed, obj.isAccelerationStructure);
);
// clang-format on
