#include "VkTexture.h"

#include <Vex/Bindings.h>
#include <Vex/RHIBindings.h>

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

} // namespace VkTextureUtil

//
// ::vk::Sampler GetOrCreateAnisotropicSamplers(NonNullPtr<VkGPUContext> ctx)
// {
//     static ::vk::UniqueSampler sampler = [&]()
//     {
//         auto properties = ctx.physDevice.getProperties();
//
//         ::vk::SamplerCreateInfo samplerCreate{
//             .magFilter = ::vk::Filter::eLinear,
//             .minFilter = ::vk::Filter::eLinear,
//             .mipmapMode = ::vk::SamplerMipmapMode::eLinear,
//             .addressModeU = ::vk::SamplerAddressMode::eRepeat,
//             .addressModeV = ::vk::SamplerAddressMode::eRepeat,
//             .addressModeW = ::vk::SamplerAddressMode::eRepeat,
//             .mipLodBias = 0,
//             .anisotropyEnable = ::vk::True,
//             .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
//             .compareEnable = ::vk::False,
//             .compareOp = ::vk::CompareOp::eAlways,
//             .minLod = 0,
//             .maxLod = 0,
//             .borderColor = ::vk::BorderColor::eIntOpaqueBlack,
//             .unnormalizedCoordinates = ::vk::False,
//         };
//
//         return VEX_VK_CHECK <<= ctx.device.createSamplerUnique(samplerCreate);
//     }();
//     return *sampler;
// }

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& inDescription, ::vk::Image backbufferImage)
    : ctx(ctx)
    , isBackBuffer(true)
    , image{ backbufferImage }
{
    description = std::move(inDescription);
    SetDebugName(ctx->device, backbufferImage, description.name.c_str());
}

VkTexture::VkTexture(const NonNullPtr<VkGPUContext> ctx,
                     const TextureDescription& inDescription,
                     ::vk::UniqueImage rawImage)
    : ctx(ctx)
    , isBackBuffer(false)
    , image{ std::move(rawImage) }
{
    description = inDescription;
    SetDebugName(ctx->device, *rawImage, description.name.c_str());
}

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, TextureDescription&& inDescription, ::vk::UniqueImage rawImage)
    : ctx(ctx)
    , isBackBuffer(false)
    , image{ std::move(rawImage) }
{
    description = std::move(inDescription);
    SetDebugName(ctx->device, *rawImage, description.name.c_str());
}

VkTexture::VkTexture(NonNullPtr<VkGPUContext> ctx, RHIAllocator& allocator, TextureDescription&& inDescription)
    : RHITextureBase(allocator)
    , ctx(ctx)
    , isBackBuffer(false)
{
    description = std::move(inDescription);
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
    VkTextureViewDesc view{
        .viewType = TextureUtil::GetTextureViewType(binding),
        .format = TextureUtil::GetTextureFormat(binding),
        .usage = static_cast<TextureUsage::Type>(binding.usage),
        .mipBias = binding.mipBias,
        .mipCount = (binding.mipCount == 0) ? description.mips : binding.mipCount,
        .startSlice = binding.startSlice,
        .sliceCount = (binding.sliceCount == 0) ? description.depthOrArraySize : binding.sliceCount,
    };
    if (auto it = bindlessCache.find(view); it != bindlessCache.end() && descriptorPool.IsValid(it->second.handle))
    {
        return it->second.handle;
    }

    const ::vk::ImageViewCreateInfo viewCreate{ .image = GetResource(),
                                                .viewType = TextureTypeToVulkan(view.viewType),
                                                .format = TextureFormatToVulkan(view.format),
                                                .subresourceRange = {
                                                    .aspectMask = VkTextureUtil::GetFormatAspectFlags(view.format),
                                                    .baseMipLevel = view.mipBias,
                                                    .levelCount = view.mipCount,
                                                    .baseArrayLayer = view.startSlice,
                                                    .layerCount = view.sliceCount,
                                                } };

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
        VEX_LOG(Fatal, "Unsupported binding usage for texture {}.", binding.texture.description.name);
    }

    descriptorPool.UpdateDescriptor(
        handle,
        ::vk::DescriptorImageInfo{ .sampler = nullptr, .imageView = *imageView, .imageLayout = viewLayout },
        view.usage & TextureUsage::ShaderReadWrite);

    bindlessCache[view] = { .handle = handle, .view = std::move(imageView) };

    return handle;
}

::vk::ImageView VkTexture::GetOrCreateImageView(const TextureBinding& binding, TextureUsage::Type usage)
{
    VkTextureViewDesc view{
        .viewType = TextureUtil::GetTextureViewType(binding),
        .format = TextureUtil::GetTextureFormat(binding),
        .usage = usage,
        .mipBias = binding.mipBias,
        .mipCount = (binding.mipCount == 0) ? description.mips : binding.mipCount,
        .startSlice = binding.startSlice,
        .sliceCount = (binding.sliceCount == 0) ? description.depthOrArraySize : binding.sliceCount,
    };
    if (auto it = viewCache.find(view); it != viewCache.end())
    {
        return *it->second;
    }

    const ::vk::ImageViewCreateInfo viewCreate{ .image = GetResource(),
                                                .viewType = TextureTypeToVulkan(view.viewType),
                                                .format = TextureFormatToVulkan(view.format),
                                                .subresourceRange = {
                                                    .aspectMask = usage == TextureUsage::DepthStencil
                                                                      ? VkTextureUtil::GetDepthAspectFlags(view.format)
                                                                      : ::vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = view.mipBias,
                                                    .levelCount = view.mipCount,
                                                    .baseArrayLayer = view.startSlice,
                                                    .layerCount = view.sliceCount,
                                                } };

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx->device.createImageViewUnique(viewCreate);

    const ::vk::ImageView ret = *imageView;
    viewCache[view] = std::move(imageView);
    return ret;
}

RHITextureState VkTexture::GetClearTextureState()
{
    return RHITextureState::ShaderReadWrite;
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

std::span<u8> VkTexture::Map()
{
    if (!allocator)
    {
        VEX_LOG(Fatal, "Texture {} cannot be mapped to", description.name);
    }
    return allocator->MapAllocation(allocation);
}

void VkTexture::Unmap()
{
    if (!allocator)
    {
        VEX_LOG(Fatal, "Texture {} cannot be unmapped", description.name);
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
    createInfo.format = TextureFormatToVulkan(description.format);
    createInfo.sharingMode = ::vk::SharingMode::eExclusive;
    createInfo.tiling = ::vk::ImageTiling::eOptimal;
    createInfo.initialLayout = GetLayout();
    createInfo.mipLevels = description.mips;
    createInfo.samples = ::vk::SampleCountFlagBits::e1;

    switch (description.type)
    {
    case TextureType::Texture2D:
        createInfo.extent = ::vk::Extent3D{ description.width, description.height, 1 };
        createInfo.arrayLayers = description.depthOrArraySize;
        createInfo.imageType = ::vk::ImageType::e2D;
        break;
    case TextureType::TextureCube:
        createInfo.extent = ::vk::Extent3D{ description.width, description.height, 1 };
        createInfo.imageType = ::vk::ImageType::e2D;
        createInfo.arrayLayers = 6;
        break;
    case TextureType::Texture3D:
        createInfo.extent = ::vk::Extent3D{ description.width, description.height, description.depthOrArraySize };
        createInfo.imageType = ::vk::ImageType::e3D;
        break;
    default:;
    }

    if (description.usage & TextureUsage::DepthStencil)
    {
        createInfo.usage |= ::vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    if (description.usage & TextureUsage::ShaderRead)
    {
        createInfo.usage |= ::vk::ImageUsageFlagBits::eSampled;
    }
    if (description.usage & TextureUsage::ShaderReadWrite)
    {
        createInfo.usage |= ::vk::ImageUsageFlagBits::eStorage;
    }
    if (description.usage & TextureUsage::RenderTarget)
    {
        createInfo.usage |= ::vk::ImageUsageFlagBits::eColorAttachment;
    }
    createInfo.usage |= ::vk::ImageUsageFlagBits::eTransferDst;
    createInfo.usage |= ::vk::ImageUsageFlagBits::eTransferSrc;

    ::vk::UniqueImage imageTmp = VEX_VK_CHECK <<= ctx->device.createImageUnique(createInfo);

    ::vk::MemoryRequirements imageMemoryReq = ctx->device.getImageMemoryRequirements(*imageTmp);

#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    auto [memory, newAllocation] = allocator.AllocateResource(description.memoryLocality, imageMemoryReq);
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
    SetDebugName(ctx->device, *memory, std::format("{}_Memory", description.name).c_str());
    VEX_VK_CHECK << ctx->device.bindImageMemory(*imageTmp, *memory, 0);
#endif
    SetDebugName(ctx->device, imageTmp.get(), description.name.c_str());

    image = std::move(imageTmp);
}

} // namespace vex::vk

namespace vex::TextureUtil
{

::vk::ImageLayout TextureStateFlagToImageLayout(RHITextureState flags)
{
    using enum RHITextureState;
    using enum ::vk::ImageLayout;

    switch (flags)
    {
    case RenderTarget:
        return eColorAttachmentOptimal;
    case ShaderReadWrite:
        return eGeneral;
    case ShaderResource:
        return eShaderReadOnlyOptimal;
    case DepthRead:
        return eDepthReadOnlyOptimal;
    case DepthWrite:
        return eDepthAttachmentOptimal;
    case CopySource:
        return eTransferSrcOptimal;
    case CopyDest:
        return eTransferDstOptimal;
    case Present:
        return ePresentSrcKHR;
    case Common:
        return eUndefined;
    default:
        VEX_ASSERT(false, "Flag to layout conversion not supported");
        return eUndefined;
    };
    return eUndefined;
}

} // namespace vex::TextureUtil