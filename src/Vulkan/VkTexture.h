#pragma once

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

    u32 mipBias = 0;
    u32 mipCount = 1;
    u32 startSlice = 0;
    u32 sliceCount = 1;

    bool operator==(const VkTextureViewDesc&) const = default;
};

} // namespace vex::vk

// clang-format off
VEX_MAKE_HASHABLE(vex::vk::VkTextureViewDesc,
    VEX_HASH_COMBINE_ENUM(seed, obj.viewType);
    VEX_HASH_COMBINE_ENUM(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.mipBias);
    VEX_HASH_COMBINE(seed, obj.mipCount);
    VEX_HASH_COMBINE(seed, obj.startSlice);
    VEX_HASH_COMBINE(seed, obj.sliceCount);
);
// clang-format on

namespace vex::vk
{
struct VkGPUContext;
struct VkDescriptorPool;

class VkBackbufferTexture : public RHITexture
{
public:
    VkBackbufferTexture(TextureDescription&& description, ::vk::Image backbufferImage);

    ::vk::Image image;
};

class VkTexture final : public RHITexture
{
public:
    // Takes ownership of the image
    VkTexture(const TextureDescription& description, ::vk::UniqueImage rawImage);
    VkTexture(TextureDescription&& description, ::vk::UniqueImage rawImage);

    // Creates a new image
    // ...
    VkTexture(VkGPUContext& ctx, TextureDescription&& description);

    BindlessHandle GetOrCreateBindlessView(VkGPUContext& device,
                                           const VkTextureViewDesc& view,
                                           VkDescriptorPool& descriptorPool);

    ::vk::Image GetResource()
    {
        return *image;
    };
    [[nodiscard]] ::vk::ImageLayout GetLayout() const
    {
        return imageLayout;
    };

private:
    void CreateImage(VkGPUContext& ctx);
    ::vk::UniqueImage image;
    ::vk::UniqueDeviceMemory memory;
    ::vk::ImageLayout imageLayout = ::vk::ImageLayout::eUndefined;

    struct CacheEntry
    {
        BindlessHandle handle = GInvalidBindlessHandle;
        ::vk::UniqueImageView view;
    };
    std::unordered_map<VkTextureViewDesc, CacheEntry> cache;

    friend class VkCommandList;
};

} // namespace vex::vk