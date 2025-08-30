#pragma once

#include <array>
#include <regex>
#include <span>
#include <utility>

#include <Vex/CommandQueueType.h>
#include <Vex/RHIFwd.h>
#include <Vex/Types.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

#include "DX12/RHI/DX12Buffer.h"
#include "DX12/RHI/DX12Texture.h"

namespace vex
{
struct RHIDrawResources;
struct DrawResources;
struct ConstantBinding;
struct ResourceBinding;
struct RHITextureBinding;
struct RHIBufferBinding;
struct InputAssembly;

//TODO: Find appropriate place for this
struct BufferToTextureCopyMapping
{
    BufferRegion srcRegion;
    TextureRegion dstRegion;
    TextureRegionExtent extent;
};

class RHICommandListBase
{
public:
    virtual bool IsOpen() const = 0;

    virtual void Open() = 0;
    virtual void Close() = 0;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) = 0;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) = 0;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) = 0;
    virtual void SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState) = 0;

    virtual void SetLayout(RHIResourceLayout& layout, RHIDescriptorPool& descriptorPool) = 0;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) = 0;
    virtual void SetInputAssembly(InputAssembly inputAssembly) = 0;

    virtual void ClearTexture(const RHITextureBinding& binding,
                              TextureUsage::Type usage,
                              const TextureClearValue& clearValue) = 0;

    virtual void Transition(RHITexture& texture, RHITextureState::Flags newState) = 0;
    virtual void Transition(RHIBuffer& texture, RHIBufferState::Flags newState) = 0;
    // Ideal for batching multiple resource transitions together.
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) = 0;
    virtual void Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs) = 0;

    // Need to be called before and after all draw commands with the same DrawBinding
    virtual void BeginRendering(const RHIDrawResources& resources) = 0;
    virtual void EndRendering() = 0;

    virtual void Draw(u32 vertexCount) = 0;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) = 0;

    virtual void TraceRays(const std::array<u32, 3>& widthHeightDepth,
                           const RHIRayTracingPipelineState& rayTracingPipelineState) = 0;

    // Copies the whole texture data from src to dst. These textures should have the same size, mips, arrayLayers, type, etc...
    virtual void Copy(RHITexture& src, RHITexture& dst);
    // Copies the regions from src to dst
    virtual void Copy(RHITexture& src, RHITexture& dst, const std::vector<TextureToTextureCopyRegionMapping>& regionMappings) = 0;
    // Copies the whole contents of src to dst. Buffers need to have the same byte size
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst);
    // Copies the buffer region from src to dst
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst, const BufferToBufferCopyRegion& regionMappings) = 0;
    // Copies the complete contents of the buffer to all regions of the dst texture.
    // This assumes the texture regions are contiguously arranged in the buffer. The whole content of the texture must be in the buffer
    virtual void Copy(RHIBuffer& src, RHITexture& dst);
    // Copies the different regions of buffers to dst texture regions
    virtual void Copy(RHIBuffer& src, RHITexture& dst, const std::vector<BufferToTextureCopyMapping>& regionMappings) = 0;

    virtual CommandQueueType GetType() const = 0;
};

// TODO: Move this to CPP file
inline void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    const auto desc = src.GetDescription();
    std::vector<std::pair<TextureRegion, TextureRegionExtent>> regions;
    u32 width = desc.width;
    u32 height = desc.height;
    u32 depth = desc.depthOrArraySize;
    for (u32 i = 0; i < desc.mips; ++i)
    {
        regions.emplace_back(
            TextureRegion{
                .mip = i,
                .baseLayer = 0,
                .layerCount = desc.type == TextureType::Texture3D ? desc.depthOrArraySize : 1,
                .offset = { 0, 0, 0 }
            },
            TextureRegionExtent{ width, height, depth }
        );

        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
        depth = std::max(1u, depth / 2u);
    }

    std::vector<TextureToTextureCopyRegionMapping> regionMappings;
    for (const auto& [region, extent] : regions)
    {
        regionMappings.emplace_back(region, region, extent);
    }

    Copy(src, dst, regionMappings);
}

inline void RHICommandListBase::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    Copy(src, dst, BufferToBufferCopyRegion{ 0, 0, src.GetDescription().byteSize });
}

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

inline void RHICommandListBase::Copy(RHIBuffer& src, RHITexture& dst)
{
    const TextureDescription& desc = dst.GetDescription();

    const u8 texelByteSize = GetPixelByteSizeFromFormat(desc.format);

    TextureRegionExtent mipSize{ desc.width,
                                desc.height,
                                (desc.type == TextureType::Texture3D) ? desc.depthOrArraySize : 1 };

    std::vector<BufferToTextureCopyMapping> regions;

    u32 bufferOffset = 0;
    for (u8 i = 0; i < desc.mips; ++i)
    {
        const u32 mipByteSize = mipSize.width * mipSize.height * (desc.type == TextureType::Texture3D
                                    ? mipSize.depth
                                    : desc.depthOrArraySize * texelByteSize);

        regions.push_back(BufferToTextureCopyMapping{
            .srcRegion = BufferRegion{ bufferOffset, mipByteSize },
            .dstRegion = TextureRegion{
                .mip = i,
                .baseLayer = 0,
                .layerCount = (desc.type == TextureType::Texture3D) ? 1 : desc.depthOrArraySize,
                .offset = { 0, 0, 0 }
            },
            .extent = { mipSize.width, mipSize.height, mipSize.depth }
        });

        bufferOffset += mipByteSize;
        mipSize = TextureRegionExtent{
            std::max(1u, mipSize.width / 2u),
            std::max(1u, mipSize.height / 2u),
            std::max(1u, mipSize.depth / 2u)
        };
    }

    Copy(src, dst, regions);
}

} // namespace vex