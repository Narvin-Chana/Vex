#pragma once

#include <string>

#include <Vex/FeatureChecker.h>

#include "VkHeaders.h"

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
    virtual std::string_view GetPhysicalDeviceName() override;
    virtual bool IsFeatureSupported(Feature feature) override;
    virtual FeatureLevel GetFeatureLevel() override;
    virtual ResourceBindingTier GetResourceBindingTier() override;
    virtual ShaderModel GetShaderModel() override;

private:
    ::vk::PhysicalDeviceProperties deviceProperties;
    ::vk::PhysicalDeviceFeatures deviceFeatures;
    ::vk::PhysicalDeviceVulkan12Features vulkan12Features;
    ::vk::PhysicalDeviceVulkan13Features vulkan13Features;
    ::vk::PhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures;
    ::vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures;
};
} // namespace vex::vk