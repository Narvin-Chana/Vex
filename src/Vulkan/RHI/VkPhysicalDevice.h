#pragma once

#include <Vex/Formats.h>
#include <Vex/Types.h>

#include <RHI/RHIPhysicalDevice.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkPhysicalDevice final : public RHIPhysicalDeviceBase
{
public:
    VkPhysicalDevice(const ::vk::PhysicalDevice& dev);
    ~VkPhysicalDevice() = default;

    VkPhysicalDevice(const VkPhysicalDevice&) = delete;
    VkPhysicalDevice& operator=(const VkPhysicalDevice&) = delete;

    VkPhysicalDevice(VkPhysicalDevice&&) = default;
    VkPhysicalDevice& operator=(VkPhysicalDevice&&) = default;

    static double GetDeviceVRAMSize(const ::vk::PhysicalDevice& physicalDevice);

    ::vk::PhysicalDevice physicalDevice;

    virtual bool IsFeatureSupported(Feature feature) const override;
    virtual FeatureLevel GetFeatureLevel() const override;
    virtual ResourceBindingTier GetResourceBindingTier() const override;
    virtual ShaderModel GetShaderModel() const override;
    virtual u32 GetMaxLocalConstantsByteSize() const override;
    bool FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const override;

    std::string_view GetMaxSupportedSpirVVersion() const;
    std::string_view GetMaxSupportedVulkanVersion() const;
    bool SupportsMinimalRequirements() const override;

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