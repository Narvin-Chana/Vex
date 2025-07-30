#include "VkTexture.h"

#include <Vulkan/RHI/VkCommandPool.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkMemory.h>

namespace vex::vk
{

::vk::ImageViewType TextureTypeToVulkan(TextureViewType type)
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
//
// ::vk::Sampler GetOrCreateAnisotropicSamplers(VkGPUContext& ctx)
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

VkBackbufferTexture::VkBackbufferTexture(TextureDescription&& inDescription, ::vk::Image backbufferImage)
    : image{ backbufferImage }
{
    description = std::move(inDescription);
}

VkImageTexture::VkImageTexture(const TextureDescription& inDescription, ::vk::UniqueImage rawImage)
    : image{ std::move(rawImage) }
{
    description = inDescription;
}

VkImageTexture::VkImageTexture(TextureDescription&& inDescription, ::vk::UniqueImage rawImage)
    : image{ std::move(rawImage) }
{
    description = std::move(inDescription);
}

VkImageTexture::VkImageTexture(VkGPUContext& ctx, TextureDescription&& inDescription)
{
    description = std::move(inDescription);
    CreateImage(ctx);
}

BindlessHandle VkTexture::GetOrCreateBindlessView(VkGPUContext& ctx,
                                                  const VkTextureViewDesc& view,
                                                  VkDescriptorPool& descriptorPool)
{
    if (auto it = cache.find(view); it != cache.end() && descriptorPool.IsValid(it->second.handle))
    {
        return it->second.handle;
    }

    const ::vk::ImageViewCreateInfo viewCreate{ .image = GetResource(),
                                                .viewType = TextureTypeToVulkan(view.viewType),
                                                .format = TextureFormatToVulkan(view.format),
                                                .subresourceRange = {
                                                    .aspectMask = ::vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = view.mipBias,
                                                    .levelCount = view.mipCount,
                                                    .baseArrayLayer = view.startSlice,
                                                    .layerCount = view.sliceCount,
                                                } };

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx.device.createImageViewUnique(viewCreate);
    const BindlessHandle handle =
        descriptorPool.AllocateStaticDescriptor(*this, view.usage & TextureUsage::ShaderReadWrite);

    descriptorPool.UpdateDescriptor(
        ctx,
        handle,
        ::vk::DescriptorImageInfo{ .sampler = nullptr, .imageView = *imageView, .imageLayout = GetLayout() });

    cache[view] = { .handle = handle, .view = std::move(imageView) };

    return handle;
}

void VkTexture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (auto& [handle, view] : cache | std::views::values)
    {
        if (handle != GInvalidBindlessHandle)
        {
            reinterpret_cast<VkDescriptorPool&>(descriptorPool).FreeStaticDescriptor(handle);
        }
    }
    cache.clear();
}

void VkImageTexture::CreateImage(VkGPUContext& ctx)
{
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

    image = VEX_VK_CHECK <<= ctx.device.createImageUnique(createInfo);

    ::vk::MemoryRequirements imageMemoryReq = ctx.device.getImageMemoryRequirements(*image);

    // memory allocation should be done elsewhere in a central place
    ::vk::MemoryAllocateInfo allocateInfo{
        .allocationSize = imageMemoryReq.size,
        .memoryTypeIndex = GetBestMemoryType(ctx.physDevice,
                                             imageMemoryReq.memoryTypeBits,
                                             ::vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = VEX_VK_CHECK <<= ctx.device.allocateMemoryUnique(allocateInfo);
    VEX_VK_CHECK << ctx.device.bindImageMemory(*image, *memory, 0);
}

} // namespace vex::vk

namespace vex::TextureUtil
{

::vk::ImageLayout TextureStateFlagToImageLayout(RHITextureState::Flags flags)
{
    using namespace RHITextureState;

    switch (flags)
    {
    case Common:
        return ::vk::ImageLayout::eUndefined;
        break;
    case RenderTarget:
        return ::vk::ImageLayout::eColorAttachmentOptimal;
        break;
    case ShaderReadWrite:
        return ::vk::ImageLayout::eGeneral;
        break;
    case ShaderResource:
        return ::vk::ImageLayout::eShaderReadOnlyOptimal;
        break;
    case DepthRead:
        return ::vk::ImageLayout::eDepthReadOnlyOptimal;
        break;
    case DepthWrite:
        return ::vk::ImageLayout::eDepthAttachmentOptimal;
        break;
    case CopySource:
        return ::vk::ImageLayout::eTransferSrcOptimal;
        break;
    case CopyDest:
        return ::vk::ImageLayout::eTransferDstOptimal;
        break;
    case Present:
        return ::vk::ImageLayout::ePresentSrcKHR;
        break;
    default:
        VEX_ASSERT(false, "Flag to layout conversion not supported");
    };
    return ::vk::ImageLayout::eUndefined;
}

} // namespace vex::TextureUtil