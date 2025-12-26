#include "VkTexture.h"

#include <Vex/Bindings.h>

#include <RHI/RHIBindings.h>

#include <Vulkan/RHI/VkAllocator.h>
#include <Vulkan/RHI/VkCommandPool.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

static ::vk::ImageViewType TextureTypeToVulkan(TextureViewType type)
{
    switch (type)
    {
    case TextureViewType::Texture2D:
        return ::vk::ImageViewType::e2D;
        break;
    case TextureViewType::Texture2DArray:
        return ::vk::ImageViewType::e2DArray;
        break;
    case TextureViewType::TextureCube:
        return ::vk::ImageViewType::eCube;
        break;
    case TextureViewType::TextureCubeArray:
        return ::vk::ImageViewType::eCubeArray;
        break;
    case TextureViewType::Texture3D:
        return ::vk::ImageViewType::e3D;
        break;
    default:
        VEX_ASSERT(false);
    }
    std::unreachable();
}

static ::vk::ImageUsageFlags GetImageUsage(const TextureDesc& desc)
{
    ::vk::ImageUsageFlags usageFlags{};
    if (desc.usage & TextureUsage::DepthStencil)
    {
        usageFlags |= ::vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    if (desc.usage & TextureUsage::ShaderRead)
    {
        usageFlags |= ::vk::ImageUsageFlagBits::eSampled;
    }
    if (desc.usage & TextureUsage::ShaderReadWrite)
    {
        usageFlags |= ::vk::ImageUsageFlagBits::eStorage;
    }
    if (desc.usage & TextureUsage::RenderTarget)
    {
        usageFlags |= ::vk::ImageUsageFlagBits::eColorAttachment;
    }
    usageFlags |= ::vk::ImageUsageFlagBits::eTransferDst;
    usageFlags |= ::vk::ImageUsageFlagBits::eTransferSrc;
    return usageFlags;
}

namespace VkTextureUtil
{

::vk::ImageAspectFlags GetDepthAspectFlags(TextureFormat format)
{
    ::vk::ImageAspectFlags aspectFlags{};
    using enum TextureFormat;
    switch (format)
    {
    case D24_UNORM_S8_UINT:
    case D32_FLOAT_S8_UINT:
        aspectFlags |= ::vk::ImageAspectFlagBits::eStencil;
    case D16_UNORM:
    case D32_FLOAT:
        aspectFlags |= ::vk::ImageAspectFlagBits::eDepth;
    default:
        break;
    }
    return aspectFlags;
}

::vk::ImageAspectFlags BindingAspectToVkAspectFlags(TextureAspect::Type aspect)
{
    switch (aspect)
    {
    case TextureAspect::Depth:
        return ::vk::ImageAspectFlagBits::eDepth;
    case TextureAspect::Stencil:
        return ::vk::ImageAspectFlagBits::eStencil;
    default:
        return ::vk::ImageAspectFlagBits::eColor;
    }
}

::vk::ImageAspectFlags GetFormatAspectFlags(TextureFormat format)
{
    ::vk::ImageAspectFlags aspectFlags{};
    using enum TextureFormat;
    switch (format)
    {
    case D24_UNORM_S8_UINT:
    case D32_FLOAT_S8_UINT:
        aspectFlags |= ::vk::ImageAspectFlagBits::eStencil;
    case D16_UNORM:
    case D32_FLOAT:
        aspectFlags |= ::vk::ImageAspectFlagBits::eDepth;
        return aspectFlags;
    default:
        return ::vk::ImageAspectFlagBits::eColor;
    }
}

::vk::ImageAspectFlags AspectFlagFromPlaneIndex(TextureFormat format, u32 plane)
{
    if (FormatUtil::IsDepthOrStencilFormat(format))
    {
        VEX_ASSERT(plane <= 1);
        if (plane == 1)
        {
            return ::vk::ImageAspectFlagBits::eStencil;
        }
        return ::vk::ImageAspectFlagBits::eDepth;
    }

    VEX_ASSERT(plane == 0);
    return ::vk::ImageAspectFlagBits::eColor;
}

} // namespace VkTextureUtil

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& inDescription, ::vk::Image backbufferImage)
    : ctx(ctx)
    , isBackBuffer(true)
    , image{ backbufferImage }
{
    desc = std::move(inDescription);
    SetDebugName(ctx->device, backbufferImage, std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name).c_str());
}

VkTexture::VkTexture(const NonNullPtr<VkGPUContext> ctx, const TextureDesc& inDescription, ::vk::UniqueImage rawImage)
    : ctx(ctx)
    , isBackBuffer(false)
    , image{ std::move(rawImage) }
{
    desc = inDescription;
    SetDebugName(ctx->device,
                 *rawImage,
                 std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name).c_str());
}

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDesc&& inDescription, ::vk::UniqueImage rawImage)
    : ctx(ctx)
    , isBackBuffer(false)
    , image{ std::move(rawImage) }
{
    desc = std::move(inDescription);
    SetDebugName(ctx->device, *rawImage, std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name).c_str());
}

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, RHIAllocator& allocator, TextureDesc&& inDescription)
    : RHITextureBase(allocator)
    , ctx(ctx)
    , isBackBuffer(false)
{
    desc = std::move(inDescription);
    CreateImage(allocator);
}

::vk::Image VkTexture::GetResource()
{
    ::vk::Image returnVal;
    std::visit(
        [this, &returnVal](auto& val)
        {
            using T = std::remove_cvref_t<decltype(val)>;
            if constexpr (std::is_same_v<T, ::vk::UniqueImage>)
            {
                returnVal = *val;
            }
            else if constexpr (std::is_same_v<T, ::vk::Image>)
            {
                returnVal = val;
            }
            else
            {
                VEX_LOG(Fatal, "Unsupported type for VkTexture::GetResource()");
            }
        },
        image);
    return returnVal;
}

BindlessHandle VkTexture::GetOrCreateBindlessView(const TextureBinding& binding, RHIDescriptorPool& descriptorPool)
{
    VkTextureViewDesc view{ binding };
    if (auto it = bindlessCache.find(view); it != bindlessCache.end() && descriptorPool.IsValid(it->second.handle))
    {
        return it->second.handle;
    }

    ::vk::ImageViewUsageCreateInfo viewUsageInfo{};
    ::vk::ImageUsageFlags viewUsage = GetImageUsage(desc);
    // If creating an sRGB view, it can't have storage usage
    if (binding.isSRGB)
    {
        viewUsage &= ~::vk::ImageUsageFlagBits::eStorage;
    }
    viewUsageInfo.usage = viewUsage;

    const ::vk::ImageViewCreateInfo viewCreate{
        .pNext = &viewUsageInfo, 
        .image = GetResource(),
        .viewType = TextureTypeToVulkan(view.viewType),
        .format = view.format,
        .subresourceRange = {
            .aspectMask = VkTextureUtil::BindingAspectToVkAspectFlags(binding.subresource.GetSingleAspect()),
            .baseMipLevel = view.subresource.startMip,
            .levelCount = view.subresource.GetMipCount(binding.texture.desc),
            .baseArrayLayer = view.subresource.startSlice,
            .layerCount = view.subresource.GetSliceCount(binding.texture.desc),
        },
    };

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx->device.createImageViewUnique(viewCreate);
    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();

    ::vk::ImageLayout viewLayout;
    switch (binding.usage)
    {
    case TextureBindingUsage::ShaderRead:
        viewLayout = ::vk::ImageLayout::eShaderReadOnlyOptimal;
        break;
    case TextureBindingUsage::ShaderReadWrite:
        viewLayout = ::vk::ImageLayout::eGeneral;
        break;
    default:
        VEX_LOG(Fatal, "Unsupported binding usage for texture {}.", binding.texture.desc.name);
    }

    descriptorPool.GetBindlessSet().UpdateDescriptor(
        handle,
        ::vk::DescriptorImageInfo{ .sampler = nullptr, .imageView = *imageView, .imageLayout = viewLayout },
        view.usage & TextureUsage::ShaderReadWrite);

    bindlessCache[view] = { .handle = handle, .view = std::move(imageView) };

    return handle;
}

::vk::ImageView VkTexture::GetOrCreateImageView(const TextureBinding& binding, TextureUsage::Type usage)
{
    VkTextureViewDesc view{ binding };
    if (auto it = viewCache.find(view); it != viewCache.end())
    {
        return *it->second;
    }

    ::vk::ImageViewUsageCreateInfo viewUsageInfo{};
    ::vk::ImageUsageFlags viewUsage = GetImageUsage(desc);
    // If creating an sRGB view, it can't have storage usage.
    if (binding.isSRGB)
    {
        viewUsage &= ~::vk::ImageUsageFlagBits::eStorage;
    }
    viewUsageInfo.usage = viewUsage;

    const ::vk::ImageViewCreateInfo viewCreate{ 
        .pNext = &viewUsageInfo, 
        .image = GetResource(),
        .viewType = TextureTypeToVulkan(view.viewType),
        .format = view.format,
        .subresourceRange = {
            .aspectMask = usage == TextureUsage::DepthStencil
                                ? VkTextureUtil::GetDepthAspectFlags(
                                    VulkanToTextureFormat(view.format))
                                : ::vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = view.subresource.startMip,
            .levelCount = view.subresource.GetMipCount(binding.texture.desc),
            .baseArrayLayer = view.subresource.startSlice,
            .layerCount = view.subresource.GetSliceCount(binding.texture.desc),
        }, 
    };

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx->device.createImageViewUnique(viewCreate);

    const ::vk::ImageView ret = *imageView;
    viewCache[view] = std::move(imageView);
    return ret;
}

void VkTexture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (const auto& [viewDesc, entry] : bindlessCache)
    {
        if (entry.handle != GInvalidBindlessHandle)
        {
            descriptorPool.FreeStaticDescriptor(entry.handle);
        }
    }
    bindlessCache.clear();
}

void VkTexture::FreeAllocation(RHIAllocator& allocator)
{
    allocator.FreeResource(allocation);
}

Span<byte> VkTexture::Map()
{
    if (!allocator)
    {
        VEX_LOG(Fatal, "Texture {} cannot be mapped to", desc.name);
    }
    return allocator->MapAllocation(allocation);
}

void VkTexture::Unmap()
{
    if (!allocator)
    {
        VEX_LOG(Fatal, "Texture {} cannot be unmapped", desc.name);
    }
    allocator->UnmapAllocation(allocation);
}

void VkTexture::CreateImage(RHIAllocator& allocator)
{
    if (isBackBuffer)
    {
        VEX_LOG(Fatal, "Calling create texture with a backbuffer is not valid behavior.");
        return;
    }

    ::vk::ImageCreateInfo createInfo{};
    // Force the non-SRGB variant.
    createInfo.format = TextureFormatToVulkan(desc.format, false);
    createInfo.sharingMode = ::vk::SharingMode::eExclusive;
    createInfo.tiling = ::vk::ImageTiling::eOptimal;
    createInfo.initialLayout = ::vk::ImageLayout::eUndefined;
    createInfo.mipLevels = desc.mips;
    createInfo.samples = ::vk::SampleCountFlagBits::e1;
    createInfo.flags = ::vk::ImageCreateFlagBits::eMutableFormat;

    switch (desc.type)
    {
    case TextureType::Texture2D:
        createInfo.extent = ::vk::Extent3D{ desc.width, desc.height, 1 };
        createInfo.imageType = ::vk::ImageType::e2D;
        createInfo.arrayLayers = desc.depthOrSliceCount;
        break;
    case TextureType::TextureCube:
        createInfo.extent = ::vk::Extent3D{ desc.width, desc.height, 1 };
        createInfo.imageType = ::vk::ImageType::e2D;
        createInfo.arrayLayers = desc.GetSliceCount();
        break;
    case TextureType::Texture3D:
        createInfo.extent = ::vk::Extent3D{ desc.width, desc.height, desc.depthOrSliceCount };
        createInfo.imageType = ::vk::ImageType::e3D;
        createInfo.arrayLayers = 1;
        break;
    default:;
    }

    createInfo.usage = GetImageUsage(desc);

    ::vk::UniqueImage imageTmp = VEX_VK_CHECK <<= ctx->device.createImageUnique(createInfo);

    ::vk::MemoryRequirements imageMemoryReq = ctx->device.getImageMemoryRequirements(*imageTmp);

#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    auto [memory, newAllocation] = allocator.AllocateResource(desc.memoryLocality, imageMemoryReq);
    allocation = newAllocation;
    VEX_VK_CHECK << ctx->device.bindImageMemory(*imageTmp, memory, allocation.memoryRange.offset);
#else
    // memory allocation should be done elsewhere in a central place
    ::vk::MemoryAllocateInfo allocateInfo{
        .allocationSize = imageMemoryReq.size,
        .memoryTypeIndex = AllocatorUtils::GetBestSuitedMemoryTypeIndex(ctx->physDevice,
                                                                        imageMemoryReq.memoryTypeBits,
                                                                        ::vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = VEX_VK_CHECK <<= ctx->device.allocateMemoryUnique(allocateInfo);
    SetDebugName(ctx->device, *memory, std::format("Memory: {}", desc.name).c_str());
    VEX_VK_CHECK << ctx->device.bindImageMemory(*imageTmp, *memory, 0);
#endif
    SetDebugName(ctx->device,
                 imageTmp.get(),
                 std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name).c_str());

    image = std::move(imageTmp);
}

VkTextureViewDesc::VkTextureViewDesc(const TextureBinding& binding)

    : viewType{ TextureUtil::GetTextureViewType(binding) }
    , format{ TextureFormatToVulkan(binding.texture.desc.format, binding.isSRGB) }
    , usage{ static_cast<TextureUsage::Type>(binding.usage) }
    , subresource{ binding.subresource }
{
    // Resolve subresource (replacing MAX values with the actual value).
    subresource.mipCount = subresource.GetMipCount(binding.texture.desc);
    subresource.sliceCount = subresource.GetSliceCount(binding.texture.desc);
}

} // namespace vex::vk
