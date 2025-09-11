#pragma once

#include <variant>

#include <Vex/Hash.h>
#include <Vex/NonNullPtr.h>

#include <RHI/RHITexture.h>

#include <Vulkan/VkHeaders.h>

namespace vex
{
struct RHITextureBinding;
}
namespace vex::vk
{

namespace VkTextureUtil
{
::vk::ImageAspectFlags GetDepthAspectFlags(TextureFormat format);
::vk::ImageAspectFlags GetFormatAspectFlags(TextureFormat format);
} // namespace VkTextureUtil

struct VkTextureViewDesc
{
    TextureViewType viewType = TextureViewType::Texture2D;
    TextureFormat format = TextureFormat::UNKNOWN;
    TextureUsage::Type usage = TextureUsage::None;

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
::vk::ImageLayout TextureStateFlagToImageLayout(RHITextureState flags);
} // namespace vex::TextureUtil

namespace vex::vk
{
struct VkGPUContext;
class VkDescriptorPool;

class VkTexture : public RHITextureBase
{
public:
    // BackBuffer constructor:
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& description, ::vk::Image backbufferImage);

    // UniqueImage constructors:
    // Takes ownership of the image
    VkTexture(const NonNullPtr<VkGPUContext> ctx, const TextureDescription& description, ::vk::UniqueImage rawImage);
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& description, ::vk::UniqueImage rawImage);

    // Creates a new image from the description
    VkTexture(NonNullPtr<VkGPUContext> ctx, RHIAllocator& allocator, TextureDescription&& description);

    [[nodiscard]] ::vk::Image GetResource();

    [[nodiscard]] bool IsBackBufferTexture() const
    {
        return isBackBuffer;
    }

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) override;

    ::vk::ImageView GetOrCreateImageView(const TextureBinding& binding, TextureUsage::Type usage);

    virtual RHITextureState GetClearTextureState() override;

    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    [[nodiscard]] ::vk::ImageLayout GetLayout() const
    {
        return TextureUtil::TextureStateFlagToImageLayout(GetCurrentState());
    }

    virtual std::span<byte> Map() override;
    virtual void Unmap() override;

    struct CacheEntry
    {
        BindlessHandle handle = GInvalidBindlessHandle;
        ::vk::UniqueImageView view;
    };
    // TODO: could potentially combine these?
    std::unordered_map<VkTextureViewDesc, CacheEntry> bindlessCache;
    std::unordered_map<VkTextureViewDesc, ::vk::UniqueImageView> viewCache;

private:
    void CreateImage(RHIAllocator& allocator);

    NonNullPtr<VkGPUContext> ctx;

    bool isBackBuffer;
    std::variant<::vk::Image, ::vk::UniqueImage> image;

#if !VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    // Only valid if type == UniqueImage.
    ::vk::UniqueDeviceMemory memory;
#endif

    std::unordered_map<VkTextureViewDesc, CacheEntry> cache;

    friend class VkCommandList;
};

} // namespace vex::vk