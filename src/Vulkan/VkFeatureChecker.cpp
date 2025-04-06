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
    bool supportsLevel12_2 =
        // Vulkan 1.3 features that correspond to FL 12_2 requirements
        vulkan13Features.synchronization2 && vulkan13Features.dynamicRendering;

    bool supportsLevel12_1 =
        // Vulkan 1.2 features that correspond to FL 12_1 requirements
        vulkan12Features.bufferDeviceAddress && vulkan12Features.descriptorIndexing &&
        vulkan12Features.shaderSampledImageArrayNonUniformIndexing;

    // Return highest supported feature level
    if (supportsLevel12_2)
    {
        return FeatureLevel::Level_12_2;
    }
    else if (supportsLevel12_1)
    {
        return FeatureLevel::Level_12_1;
    }
    else
    {
        return FeatureLevel::Level_12_0;
    }
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
    // Tier 2: Mid-range hardware
    else if (limits.maxPerStageDescriptorSamplers >= 16 && limits.maxPerStageDescriptorUniformBuffers >= 12 &&
             limits.maxPerStageDescriptorStorageBuffers >= 16 && limits.maxPerStageDescriptorSampledImages >= 64 &&
             limits.maxPerStageDescriptorStorageImages >= 8)
    {
        return ResourceBindingTier::ResourceTier2;
    }
    // Tier 1: Basic hardware
    else
    {
        return ResourceBindingTier::ResourceTier1;
    }
}

ShaderModel VkFeatureChecker::GetShaderModel()
{
    // Map Vulkan version to feature level
    uint32_t majorVersion = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
    uint32_t minorVersion = VK_API_VERSION_MINOR(deviceProperties.apiVersion);

    // Start with base level based on Vulkan version
    ShaderModel maxShaderModel = ShaderModel::SM_6_0;

    // Vulkan 1.1 supports SM 6.0
    if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 1))
    {
        maxShaderModel = ShaderModel::SM_6_0;
    }

    // Vulkan 1.2 supports SM 6.2 (with extensions)
    if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 2))
    {
        maxShaderModel = ShaderModel::SM_6_2;

        // SM 6.4 with buffer device address
        if (vulkan12Features.bufferDeviceAddress)
        {
            maxShaderModel = ShaderModel::SM_6_4;
        }

        // SM 6.5 with ray tracing support
        if (IsFeatureSupported(Feature::RayTracing))
        {
            maxShaderModel = ShaderModel::SM_6_5;
        }
    }

    // Vulkan 1.3 adds support for SM 6.6
    if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 3))
    {
        maxShaderModel = ShaderModel::SM_6_6;

        // SM 6.7 with dynamic rendering
        if (vulkan13Features.dynamicRendering)
        {
            maxShaderModel = ShaderModel::SM_6_7;
        }
    }

    // Vulkan mesh shader extension requires SM 6.8
    if (IsFeatureSupported(Feature::MeshShader))
    {
        maxShaderModel = ShaderModel::SM_6_8;
    }

    return maxShaderModel;
}

} // namespace vex::vk