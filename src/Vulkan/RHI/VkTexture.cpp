#include "VkTexture.h"

#include <Vex/Bindings.h>
#include <Vex/RHIBindings.h>

#include <Vulkan/RHI/VkCommandPool.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkMemory.h>

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

VkTexture::VkTexture(VkGPUContext& ctx, TextureDescription&& inDescription, ::vk::Image backbufferImage)
    : ctx(&ctx)
    , type(BackBuffer)
    , image{ backbufferImage }
{
    description = std::move(inDescription);
}

VkTexture::VkTexture(VkGPUContext& ctx, const TextureDescription& inDescription, ::vk::UniqueImage rawImage)
    : ctx(&ctx)
    , type(Image)
    , image{ std::move(rawImage) }
{
    description = inDescription;
}

VkTexture::VkTexture(VkGPUContext& ctx, TextureDescription&& inDescription, ::vk::UniqueImage rawImage)
    : ctx(&ctx)
    , type(Image)
    , image{ std::move(rawImage) }
{
    description = std::move(inDescription);
}

VkTexture::VkTexture(VkGPUContext& ctx, TextureDescription&& inDescription)
    : ctx(&ctx)
    , type(Image)
{
    description = std::move(inDescription);
    CreateImage();
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

BindlessHandle VkTexture::GetOrCreateBindlessView(const ResourceBinding& binding,
                                                  TextureUsage::Type usage,
                                                  RHIDescriptorPool& descriptorPool)
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
    if (auto it = bindlessCache.find(view); it != bindlessCache.end() && descriptorPool.IsValid(it->second.handle))
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

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx->device.createImageViewUnique(viewCreate);
    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();

    descriptorPool.UpdateDescriptor(
        *ctx,
        handle,
        ::vk::DescriptorImageInfo{ .sampler = nullptr, .imageView = *imageView, .imageLayout = GetLayout() },
        view.usage & TextureUsage::ShaderReadWrite);

    bindlessCache[view] = { .handle = handle, .view = std::move(imageView) };

    return handle;
}

::vk::ImageView VkTexture::GetOrCreateImageView(const ResourceBinding& binding, TextureUsage::Type usage)
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
                                                    .aspectMask = ::vk::ImageAspectFlagBits::eColor,
                                                    .baseMipLevel = view.mipBias,
                                                    .levelCount = view.mipCount,
                                                    .baseArrayLayer = view.startSlice,
                                                    .layerCount = view.sliceCount,
                                                } };

    ::vk::UniqueImageView imageView = VEX_VK_CHECK <<= ctx->device.createImageViewUnique(viewCreate);

    ::vk::ImageView ret = *imageView;
    viewCache[view] = std::move(imageView);
    return ret;
}

RHITextureState::Type VkTexture::GetClearTextureState()
{
    if (description.usage & TextureUsage::DepthStencil)
    {
        return RHITextureState::DepthWrite;
    }

    return RHITextureState::ShaderReadWrite;
}

void VkTexture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (const auto& [handle, view] : bindlessCache | std::views::values)
    {
        if (handle != GInvalidBindlessHandle)
        {
            descriptorPool.FreeStaticDescriptor(handle);
        }
    }
    bindlessCache.clear();
}

void VkTexture::CreateImage()
{
    if (type != Image)
    {
        VEX_LOG(Fatal, "Calling create texture with an unsupported type is not valid behavior.");
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

    // memory allocation should be done elsewhere in a central place
    ::vk::MemoryAllocateInfo allocateInfo{
        .allocationSize = imageMemoryReq.size,
        .memoryTypeIndex = GetBestMemoryType(ctx->physDevice,
                                             imageMemoryReq.memoryTypeBits,
                                             ::vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = VEX_VK_CHECK <<= ctx->device.allocateMemoryUnique(allocateInfo);
    VEX_VK_CHECK << ctx->device.bindImageMemory(*imageTmp, *memory, 0);

    image = std::move(imageTmp);
}

} // namespace vex::vk

namespace vex::TextureUtil
{

::vk::ImageLayout TextureStateFlagToImageLayout(RHITextureState::Flags flags)
{
    using namespace RHITextureState;
    using enum ::vk::ImageLayout;

    switch (flags)
    {
    case Common:
        return eUndefined;
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
    default:
        VEX_LOG(Fatal, "Flag to layout conversion not supported");
    };
    std::unreachable();
}

} // namespace vex::TextureUtil