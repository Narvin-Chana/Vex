#pragma once

#include <Vex/NonNullPtr.h>

#include <RHI/RHIResourceLayout.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{
class VkDescriptorPool;
struct VkGPUContext;

class VkResourceLayout final : public RHIResourceLayoutBase
{
public:
    VkResourceLayout(NonNullPtr<VkGPUContext> ctx, const VkDescriptorPool& descriptorPool);

    ::vk::UniquePipelineLayout pipelineLayout;
};

} // namespace vex::vk