#pragma once

#include <Vex/FeatureChecker.h>

#include <Vulkan/VkHeaders.h>

namespace vk
{
class PhysicalDevice;
}

namespace vex::vk
{
class VkFeatureChecker : public vex::FeatureChecker
{
public:
    VkFeatureChecker() = default;
    VkFeatureChecker(const ::vk::PhysicalDevice& physicalDevice);
    virtual ~VkFeatureChecker() override;
    virtual bool IsFeatureSupported(Feature feature) const override;
    virtual FeatureLevel GetFeatureLevel() const override;
    virtual ResourceBindingTier GetResourceBindingTier() const override;
    virtual ShaderModel GetShaderModel() const override;
    static u32 GetMaxPushConstantSize()
    {
        return 128;
    };

private:
    ::vk::PhysicalDeviceProperties deviceProperties;
    ::vk::PhysicalDeviceFeatures deviceFeatures;
    ::vk::PhysicalDeviceVulkan12Features vulkan12Features;
    ::vk::PhysicalDeviceVulkan13Features vulkan13Features;
    ::vk::PhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures;
    ::vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures;
    ::vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures;
};
} // namespace vex::vk