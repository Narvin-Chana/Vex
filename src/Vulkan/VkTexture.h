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
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

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
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

private:
    void CreateImage(VkGPUContext& ctx);
    ::vk::UniqueImage image;
    ::vk::UniqueDeviceMemory memory;
};

} // namespace vex::vk