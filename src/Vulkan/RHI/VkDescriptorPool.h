#pragma once

#include "VkDescriptorSet.h"

#include <Vex/NonNullPtr.h>

#include <RHI/RHIDescriptorPool.h>
#include <RHI/RHIFwd.h>

#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkRHIFwd.h>

namespace vex::vk
{
struct VkGPUContext;

class VkDescriptorPool final : public RHIDescriptorPoolBase
{
public:
    VkDescriptorPool(NonNullPtr<VkGPUContext> ctx);

    virtual RHIBindlessDescriptorSet CreateBindlessSet() override;

    ::vk::DescriptorPool& GetNativeDescriptorPool()
    {
        return *descriptorPool;
    }

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueDescriptorPool descriptorPool;

    friend class VkCommandList;
    friend class VkResourceLayout;
};

} // namespace vex::vk