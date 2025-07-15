#pragma once

#include "VkDescriptorPool.h"

#include <Vex/RHI/RHITexture.h>

#include "Vex/Hash.h"
#include "Vex/RHI/RHIDescriptorPool.h"
#include "VkHeaders.h"

namespace vex::vk
{

struct VkTextureViewDesc
{
    TextureViewType viewType = TextureViewType::Texture2D;
    TextureFormat format = TextureFormat::UNKNOWN;
    ResourceUsage::Flags usage = ResourceUsage::None;

    u32 mipBias = 0;
    u32 mipCount = 1;
    u32 startSlice = 0;
    u32 sliceCount = 1;

    bool operator==(const VkTextureViewDesc&) const = default;
};

} // namespace vex::vk

// clang-format off
VEX_MAKE_HASHABLE(vex::vk::VkTextureViewDesc,
    VEX_HASH_COMBINE(seed, obj.viewType);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.mipBias);
    VEX_HASH_COMBINE(seed, obj.mipCount);
    VEX_HASH_COMBINE(seed, obj.startSlice);
    VEX_HASH_COMBINE(seed, obj.sliceCount);
);
// clang-format on

namespace vex::TextureUtil
{
::vk::ImageLayout TextureStateFlagToImageLayout(RHITextureState::Flags flags);
} // namespace vex::TextureUtil

namespace vex::vk
{
struct VkGPUContext;
class VkDescriptorPool;

class VkTexture : public RHITexture
{
public:
    virtual ::vk::Image GetResource() = 0;

    BindlessHandle GetOrCreateBindlessView(VkGPUContext& device,
                                           const VkTextureViewDesc& view,
                                           VkDescriptorPool& descriptorPool);
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

    [[nodiscard]] ::vk::ImageLayout GetLayout() const
    {
        return TextureUtil::TextureStateFlagToImageLayout(GetCurrentState());
    }

    struct CacheEntry
    {
        BindlessHandle handle = GInvalidBindlessHandle;
        ::vk::UniqueImageView view;
    };
    std::unordered_map<VkTextureViewDesc, CacheEntry> cache;
};

class VkBackbufferTexture : public VkTexture
{
public:
    VkBackbufferTexture(TextureDescription&& description, ::vk::Image backbufferImage);

    virtual ::vk::Image GetResource() override
    {
        return image;
    }

    ::vk::Image image;
};

class VkImageTexture : public VkTexture
{
public:
    // Takes ownership of the image
    VkImageTexture(const TextureDescription& description, ::vk::UniqueImage rawImage);
    VkImageTexture(TextureDescription&& description, ::vk::UniqueImage rawImage);

    // Creates a new image from the description
    VkImageTexture(VkGPUContext& ctx, TextureDescription&& description);

    virtual ::vk::Image GetResource() override
    {
        return *image;
    };

private:
    void CreateImage(VkGPUContext& ctx);
    ::vk::UniqueImage image;
    ::vk::UniqueDeviceMemory memory;

    friend class VkCommandList;
};

} // namespace vex::vk