#pragma once

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkDescriptorPool;
struct VkGPUContext;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(NonNullPtr<VkGPUContext> ctx, NonNullPtr<VkDescriptorPool> descriptorPool);

    const ::vk::UniquePipelineLayout& GetPipelineLayout() const;

private:
    ::vk::UniquePipelineLayout CreateLayout();

    NonNullPtr<VkGPUContext> ctx;
    NonNullPtr<VkDescriptorPool> descriptorPool;
    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk