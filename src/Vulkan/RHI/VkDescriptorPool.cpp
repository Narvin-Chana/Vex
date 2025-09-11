#include "VkDescriptorPool.h"

#include <Vulkan/RHI/VkDescriptorSet.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

VkDescriptorPool::VkDescriptorPool(NonNullPtr<VkGPUContext> ctx)
    : ctx{ ctx }
{
    std::array poolSize{ ::vk::DescriptorPoolSize{
                             .type = ::vk::DescriptorType::eMutableEXT,
                             .descriptorCount = GDefaultDescriptorPoolSize,
                         },
                         ::vk::DescriptorPoolSize{
                             .type = ::vk::DescriptorType::eSampler,
                             .descriptorCount = GDefaultDescriptorPoolSize,
                         } };

    ::vk::DescriptorPoolCreateInfo descriptorPoolInfo{
        .flags = ::vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 ::vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets =
            1024, // External users might need to create more descriptor sets than the bindless one (ImGui for example)
        .poolSizeCount = static_cast<u32>(poolSize.size()),
        .pPoolSizes = poolSize.data(),
    };

    descriptorPool = VEX_VK_CHECK <<= ctx->device.createDescriptorPoolUnique(descriptorPoolInfo);

    bindlessSet.emplace(ctx, *descriptorPool);
}

} // namespace vex::vk