#pragma once

#include <Vex/RHI/RHITexture.h>

#include "VkHeaders.h"

namespace vex::vk
{
struct VkGPUContext;

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

    friend class VkCommandList;
};

} // namespace vex::vk