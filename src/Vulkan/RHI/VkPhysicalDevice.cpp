#include "VkPhysicalDevice.h"

#include <Vulkan/VkFormats.h>

namespace vex::vk
{

VkPhysicalDevice::VkPhysicalDevice(const ::vk::PhysicalDevice& dev)
    : physicalDevice(dev)
{
    info.deviceName = dev.getProperties().deviceName.data();
    info.dedicatedVideoMemoryMB = GetDeviceVRAMSize(dev);

    deviceProperties = physicalDevice.getProperties();
    deviceFeatures = physicalDevice.getFeatures();

    // Get vk 1.2 features
    ::vk::PhysicalDeviceFeatures2 features2_vk12;
    physicalDevice.getFeatures2(&features2_vk12);

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

    // Get ray tracing features
    ::vk::PhysicalDeviceFeatures2 descriptorIndexingFeatures2;
    descriptorIndexingFeatures2.setPNext(&descriptorIndexingFeatures);
    physicalDevice.getFeatures2(&descriptorIndexingFeatures2);
}

double VkPhysicalDevice::GetDeviceVRAMSize(const ::vk::PhysicalDevice& physicalDevice)
{
    ::vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    double totalDeviceLocalMemoryMB = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        if (memoryProperties.memoryHeaps[i].flags & ::vk::MemoryHeapFlagBits::eDeviceLocal)
        {
            // Device local implies its VRAM memory
            VkDeviceSize heapSize = memoryProperties.memoryHeaps[i].size;

            // Convert to a more readable format (MB)
            totalDeviceLocalMemoryMB += static_cast<double>(heapSize) / (1024.0 * 1024.0);
        }
    }
    return totalDeviceLocalMemoryMB;
}

bool VkPhysicalDevice::IsFeatureSupported(Feature feature) const
{
    switch (feature)
    {
    case Feature::MeshShader:
        return meshShaderFeatures.meshShader && meshShaderFeatures.taskShader;
    case Feature::RayTracing:
        // Vulkan RHI currently does not support ray tracing.
        return rayTracingFeatures.rayTracingPipeline;
    case Feature::MipGeneration:
        // Vk can use vkCmdBlitImage to generate mips.
        return true;
    default:
        return false;
    }
    std::unreachable();
}

FeatureLevel VkPhysicalDevice::GetFeatureLevel() const
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

ResourceBindingTier VkPhysicalDevice::GetResourceBindingTier() const
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

ShaderModel VkPhysicalDevice::GetShaderModel() const
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

u32 VkPhysicalDevice::GetMaxLocalConstantsByteSize() const
{
    return deviceProperties.limits.maxPushConstantsSize;
}

std::string_view VkPhysicalDevice::GetMaxSupportedSpirVVersion() const
{
    // Based on Vulkan API version
    if (deviceProperties.apiVersion >= VK_API_VERSION_1_3)
    {
        return "spirv_1_6";
    }
    else if (deviceProperties.apiVersion >= VK_API_VERSION_1_2)
    {
        return "spirv_1_5";
    }
    else if (deviceProperties.apiVersion >= VK_API_VERSION_1_1)
    {
        return "spirv_1_3";
    }
    return "spirv_1_0";
}

std::string_view VkPhysicalDevice::GetMaxSupportedVulkanVersion() const
{
    if (deviceProperties.apiVersion >= VK_API_VERSION_1_3)
    {
        return "vulkan1.3";
    }
    else if (deviceProperties.apiVersion >= VK_API_VERSION_1_2)
    {
        return "vulkan1.2";
    }
    else if (deviceProperties.apiVersion >= VK_API_VERSION_1_1)
    {
        return "vulkan1.1";
    }
    return "vulkan1.0";
}

bool VkPhysicalDevice::SupportsMinimalRequirements() const
{
    bool supportsBindless = descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing &&
                            descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind &&
                            descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing &&
                            descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind &&
                            descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing &&
                            descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind;
    if (!supportsBindless)
    {
        return false;
    }

    if (deviceProperties.apiVersion < VK_API_VERSION_1_3)
    {
        return false;
    }
    return true;
}

bool VkPhysicalDevice::FormatSupportsLinearFiltering(TextureFormat format, bool isSRGB) const
{
    ::vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(TextureFormatToVulkan(format, isSRGB));
    return !!(formatProperties.optimalTilingFeatures & ::vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
}

} // namespace vex::vk