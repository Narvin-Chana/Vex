#pragma once

#include <variant>

#include <Vex/Hash.h>
#include <Vex/NonNullPtr.h>

#include <RHI/RHIDescriptorPool.h>
#include <RHI/RHITexture.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct RHITextureBinding;
}
namespace vex::vk
{

struct VkTextureViewDesc
{
    TextureViewType viewType = TextureViewType::Texture2D;
    TextureFormat format = TextureFormat::UNKNOWN;
    TextureUsage::Flags usage = TextureUsage::None;

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

class VkTexture : public RHITextureInterface
{
public:
    // BackBuffer constructor:
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& description, ::vk::Image backbufferImage);

    // UniqueImage constructors:
    // Takes ownership of the image
    VkTexture(const NonNullPtr<VkGPUContext> ctx, const TextureDescription& description, ::vk::UniqueImage rawImage);
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& description, ::vk::UniqueImage rawImage);

    // Creates a new image from the description
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& description);

    [[nodiscard]] ::vk::Image GetResource();

    [[nodiscard]] bool IsBackBufferTexture() const
    {
        return type == BackBuffer;
    }

    virtual BindlessHandle GetOrCreateBindlessView(const ResourceBinding& binding,
                                                   TextureUsage::Type usage,
                                                   RHIDescriptorPool& descriptorPool) override;

    ::vk::ImageView GetOrCreateImageView(const ResourceBinding& binding, TextureUsage::Type usage);

    virtual RHITextureState::Type GetClearTextureState() override;

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
    std::unordered_map<VkTextureViewDesc, CacheEntry> bindlessCache;
    std::unordered_map<VkTextureViewDesc, ::vk::UniqueImageView> viewCache;

private:
    void CreateImage();

    NonNullPtr<VkGPUContext> ctx;

    enum Type : u8
    {
        Image = 0,
        BackBuffer = 1,
    } type;
    std::variant<::vk::Image, ::vk::UniqueImage> image;

    // Only valid if type == UniqueImage.
    ::vk::UniqueDeviceMemory memory;

    std::unordered_map<VkTextureViewDesc, CacheEntry> cache;

    friend class VkCommandList;
};

} // namespace vex::vk