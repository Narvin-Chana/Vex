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
::vk::ImageAspectFlags BindingAspectToVkAspectFlags(TextureBindingAspect aspect);
::vk::ImageAspectFlags GetFormatAspectFlags(TextureFormat format);
} // namespace VkTextureUtil

struct VkTextureViewDesc
{
    VkTextureViewDesc(const TextureBinding& binding);

    TextureViewType viewType = TextureViewType::Texture2D;
    ::vk::Format format;
    TextureUsage::Type usage = TextureUsage::None;

    TextureSubresource subresource;

    bool operator==(const VkTextureViewDesc&) const = default;
};

} // namespace vex::vk

// clang-format off
VEX_MAKE_HASHABLE(vex::vk::VkTextureViewDesc,
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

class VkTexture : public RHITextureBase
{
public:
    // BackBuffer constructor:
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& desc, ::vk::Image backbufferImage);

    // UniqueImage constructors:
    // Takes ownership of the image
    VkTexture(const NonNullPtr<VkGPUContext> ctx, const TextureDesc& desc, ::vk::UniqueImage rawImage);
    VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& desc, ::vk::UniqueImage rawImage);

    // Creates a new image from the description
    VkTexture(NonNullPtr<VkGPUContext> ctx, RHIAllocator& allocator, TextureDesc&& desc);

    [[nodiscard]] ::vk::Image GetResource();

    [[nodiscard]] bool IsBackBufferTexture() const
    {
        return isBackBuffer;
    }

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) override;

    ::vk::ImageView GetOrCreateImageView(const TextureBinding& binding, TextureUsage::Type usage);

    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    virtual Span<byte> Map() override;
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