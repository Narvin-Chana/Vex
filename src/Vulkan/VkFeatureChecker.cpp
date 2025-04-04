#include "VkFeatureChecker.h"

#include <Vex/Logger.h>

namespace vex::vk
{

VkFeatureChecker::VkFeatureChecker(const ::vk::PhysicalDevice& physicalDevice)
{
    deviceProperties = physicalDevice.getProperties();
    deviceFeatures = physicalDevice.getFeatures();

    // Get vk 1.2 features
    ::vk::PhysicalDeviceFeatures2 features2_vk12;
    features2_vk12.setPNext(&vulkan12Features);
    physicalDevice.getFeatures2(&features2_vk12);

    if (deviceProperties.apiVersion < VK_API_VERSION_1_3)
    {
        VEX_LOG(Warning, "Physical device must support Vulkan 1.3. App may be unstable");
        return;
    }

    // Get vk 1.3 features
    ::vk::PhysicalDeviceFeatures2 features2_vk13;
    features2_vk13.setPNext(&vulkan13Features);
    physicalDevice.getFeatures2(&features2_vk13);

    // Get mesh shader features
    ::vk::PhysicalDeviceFeatures2 meshShaderFeatures2;
    meshShaderFeatures2.setPNext(&meshShaderFeatures);
    physicalDevice.getFeatures2(&meshShaderFeatures2);

    // Get ray tracing features
    ::vk::PhysicalDeviceFeatures2 rayTracingFeatures2;
    rayTracingFeatures2.setPNext(&rayTracingFeatures);
    physicalDevice.getFeatures2(&rayTracingFeatures2);
}

VkFeatureChecker::~VkFeatureChecker() = default;

std::string_view VkFeatureChecker::GetPhysicalDeviceName()
{
    return deviceProperties.deviceName.data();
}

bool VkFeatureChecker::IsFeatureSupported(Feature feature)
{
    switch (feature)
    {
    case Feature::MeshShader:
        return meshShaderFeatures.meshShader && meshShaderFeatures.taskShader;
    case Feature::RayTracing:
        return rayTracingFeatures.rayTracingPipeline;
    default:
        return false;
    }
    std::unreachable();
}

FeatureLevel VkFeatureChecker::GetFeatureLevel()
{
    // Map Vulkan version to feature level
    uint32_t majorVersion = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
    uint32_t minorVersion = VK_API_VERSION_MINOR(deviceProperties.apiVersion);

    // Map Vulkan 1.3 to Feature Level 12_2
    if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 3))
    {
        return FeatureLevel::Level_12_2;
    }

    // Could do the same sort of mapping, vk 1.2 and lower to FeatureLevel 12_1.
    // This is once we support vk <= 1.2.
    return FeatureLevel::Level_12_1;
}

ResourceBindingTier VkFeatureChecker::GetResourceBindingTier()
{
    // Determine resource binding tier based on device limits
    const ::vk::PhysicalDeviceLimits& limits = deviceProperties.limits;

    // Tier 3: High-end hardware with large binding count support
    if (limits.maxPerStageDescriptorSamplers >= 16 && limits.maxPerStageDescriptorUniformBuffers >= 14 &&
        limits.maxPerStageDescriptorStorageBuffers >= 64 && limits.maxPerStageDescriptorSampledImages >= 128 &&
        limits.maxPerStageDescriptorStorageImages >= 64 && limits.maxDescriptorSetSamplers >= 128 &&
        limits.maxDescriptorSetUniformBuffers >= 72 && limits.maxDescriptorSetStorageBuffers >= 128 &&
        limits.maxDescriptorSetSampledImages >= 256 && limits.maxDescriptorSetStorageImages >= 64)
    {
        return ResourceBindingTier::ResourceTier3;
    }

    // Mappings of Vulkan to DX12-like resource binding tier.
    // Doing this allows us to have a similar amount of slots in shaders.

    VEX_LOG(Fatal, "No resource binding tier matched this device: {}", GetPhysicalDeviceName());
    std::unreachable();
    //// Tier 2: Mid-range hardware
    // else if (limits.maxPerStageDescriptorSamplers >= 16 && limits.maxPerStageDescriptorUniformBuffers >= 12 &&
    //          limits.maxPerStageDescriptorStorageBuffers >= 16 && limits.maxPerStageDescriptorSampledImages >= 64 &&
    //          limits.maxPerStageDescriptorStorageImages >= 8)
    //{
    //     return ResourceBindingTier::ResourceTier2;
    // }
    //// Tier 1: Basic hardware
    // else
    //{
    //     return ResourceBindingTier::ResourceTier1;
    // }
}

ShaderModel VkFeatureChecker::GetShaderModel()
{
    // Map Vulkan version to feature level
    uint32_t majorVersion = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
    uint32_t minorVersion = VK_API_VERSION_MINOR(deviceProperties.apiVersion);

    // Map Vulkan 1.3 to Feature Level 12_2
    if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 3))
    {
        // Dynamic rendering is only supported with vk 1.3 and SM_6_7!
        return vulkan13Features.dynamicRendering ? ShaderModel::SM_6_7 : ShaderModel::SM_6_6;
    }

    VEX_LOG(Fatal, "Unable to find shader model for this version ({}.{})", majorVersion, minorVersion);
    std::unreachable();
}

} // namespace vex::vk