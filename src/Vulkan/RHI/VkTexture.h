#pragma once

#include <variant>

#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>

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
::vk::ImageAspectFlags BindingAspectToVkAspectFlags(TextureAspect aspect);
::vk::ImageAspectFlags GetFormatAspectFlags(TextureFormat format);
::vk::ImageAspectFlags AspectFlagFromPlaneIndex(TextureFormat format, u32 plane);

} // namespace VkTextureUtil

struct VkTextureView
{
    VkTextureView(const TextureBinding& binding);

    TextureViewType viewType = TextureViewType::Texture2D;
    ::vk::Format format;
    TextureUsage usage = TextureUsage::None;

    TextureSubresource subresource;

    bool operator==(const VkTextureView&) const = default;
};

} // namespace vex::vk

// clang-format off
VEX_MAKE_HASHABLE(vex::vk::VkTextureView,
    VEX_HASH_COMBINE(seed, obj.viewType);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.subresource);
);
// clang-format on

namespace vex::vk
{
struct VkGPUContext;
class VkDescriptorPool;

class VkTexture final : public RHITextureBase
{
public:
    // BackBuffer constructor:
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& desc, ::vk::Image backbufferImage);

    // UniqueImage constructors:
    // Takes ownership of the image
    VkTexture(NonNullPtr<VkGPUContext> ctx, const TextureDesc& desc, ::vk::UniqueImage rawImage);
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& desc, ::vk::UniqueImage rawImage);

    // Creates a new image from the description
    VkTexture(NonNullPtr<VkGPUContext> ctx, RHIAllocator& allocator, TextureDesc&& desc);

    [[nodiscard]] ::vk::Image GetRawTexture();

    [[nodiscard]] bool IsBackBufferTexture() const
    {
        return isBackBuffer;
    }

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) override;

    ::vk::ImageView GetOrCreateImageView(const TextureBinding& binding, TextureUsage usage);

    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    struct CacheEntry
    {
        BindlessHandle handle = GInvalidBindlessHandle;
        ::vk::UniqueImageView view;
    };
    // TODO: could potentially combine these?
    std::unordered_map<VkTextureView, CacheEntry> bindlessCache;
    std::unordered_map<VkTextureView, ::vk::UniqueImageView> viewCache;

private:
    void CreateImage(RHIAllocator& allocator);

    NonNullPtr<VkGPUContext> ctx;

    bool isBackBuffer;
#if !VEX_USE_CUSTOM_RESOURCE_ALLOCATOR
    // Only valid if type == UniqueImage.
    // Order is important, destroy the memory AFTER the image!
    ::vk::UniqueDeviceMemory memory;
#endif
    std::variant<::vk::Image, ::vk::UniqueImage> image;

    std::unordered_map<VkTextureView, CacheEntry> cache;

    friend class VkCommandList;
};

} // namespace vex::vk