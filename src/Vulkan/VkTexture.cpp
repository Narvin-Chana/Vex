#include "VkTexture.h"

#include "VkErrorHandler.h"
#include "VkFormats.h"
#include "VkGPUContext.h"
#include "VkMemory.h"

namespace vex::vk
{

VkBackbufferTexture::VkBackbufferTexture(TextureDescription&& inDescription, ::vk::Image backbufferImage)
    : image{ backbufferImage }
{
    description = std::move(inDescription);
}

VkTexture::VkTexture(const TextureDescription& inDescription, ::vk::UniqueImage rawImage)
    : image{ std::move(rawImage) }
{
    description = inDescription;
}

VkTexture::VkTexture(TextureDescription&& inDescription, ::vk::UniqueImage rawImage)
    : image{ std::move(rawImage) }
{
    description = std::move(inDescription);
}

VkTexture::VkTexture(VkGPUContext& ctx, TextureDescription&& inDescription)
{
    description = std::move(inDescription);
    CreateImage(ctx);
}

void VkTexture::CreateImage(VkGPUContext& ctx)
{
    ::vk::ImageCreateInfo createInfo{};
    createInfo.format = TextureFormatToVulkan(description.format);
    createInfo.sharingMode = ::vk::SharingMode::eExclusive;
    createInfo.tiling = ::vk::ImageTiling::eOptimal;
    createInfo.initialLayout = ::vk::ImageLayout::eUndefined;
    createInfo.mipLevels = description.mips;
    createInfo.samples = ::vk::SampleCountFlagBits::e1;
    switch (description.type)
    {
    case TextureType::Texture2D:
        createInfo.extent = { description.width, description.height };
        createInfo.arrayLayers = description.depthOrArraySize;
        createInfo.imageType = ::vk::ImageType::e2D;
        break;
    case TextureType::TextureCube:
        createInfo.extent = { description.width, description.height };
        createInfo.imageType = ::vk::ImageType::e2D;
        createInfo.arrayLayers = 6;
        break;
    case TextureType::Texture3D:
        createInfo.extent = { description.width, description.height, description.depthOrArraySize };
        createInfo.imageType = ::vk::ImageType::e3D;
        break;
    default:;
    }

    image = VEX_VK_CHECK <<= ctx.device.createImageUnique(createInfo);

    ::vk::MemoryRequirements imageMemoryReq = ctx.device.getImageMemoryRequirements(*image);

    // memory allocation should be done elsewhere in a central place
    ::vk::MemoryAllocateInfo allocateInfo{
        .allocationSize = imageMemoryReq.size,
        .memoryTypeIndex = getBestMemoryType(ctx.physDevice,
                                             imageMemoryReq.memoryTypeBits,
                                             ::vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = VEX_VK_CHECK <<= ctx.device.allocateMemoryUnique(allocateInfo);
    VEX_VK_CHECK << ctx.device.bindImageMemory(*image, *memory, 0);
}

} // namespace vex::vk
