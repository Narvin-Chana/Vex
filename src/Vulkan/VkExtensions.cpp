#include "VkExtensions.h"

#include <ranges>
#include <unordered_set>

#include <vulkan/vulkan_core.h>

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include "VkHeaders.h"

namespace vex::vk
{

std::vector<const char*> GetRequiredInstanceExtensions(bool enableGPUDebugLayer)
{
    std::vector<const char*> extensions;

    // Required for any windowed application
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    // Add validation/debug layer
    if (enableGPUDebugLayer)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Platform-specific surface extensions
#if defined(_WIN32)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#endif

    return extensions;
}

std::vector<const char*> GetDefaultValidationLayers(bool enableGPUBasedValidation)
{
    std::vector<const char*> validationLayers;

    if (enableGPUBasedValidation)
    {
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    return validationLayers;
}

std::vector<const char*> GetDefaultDeviceExtensions()
{
    return { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME };
}

std::vector<const char*> FilterSupportedValidationLayers(const std::vector<const char*>& layers)
{
    uint32_t layerCount{};
    assert(::vk::enumerateInstanceLayerProperties(&layerCount, nullptr) == ::vk::Result::eSuccess);

    std::vector<::vk::LayerProperties> availableLayers(layerCount);
    assert(::vk::enumerateInstanceLayerProperties(&layerCount, availableLayers.data()) == ::vk::Result::eSuccess);

    auto requestedLayers = layers | std::ranges::to<std::unordered_set<std::string_view>>();

    std::unordered_set<std::string_view> availableLayerSet;
    for (const auto& prop : availableLayers)
    {
        availableLayerSet.insert(prop.layerName);
    }

    std::vector<const char*> supportedLayers;
    std::vector<const char*> unsupportedLayers;
    supportedLayers.reserve(layers.size());
    for (const char* layer : layers)
    {
        if (availableLayerSet.contains(layer))
        {
            supportedLayers.push_back(layer);
        }
        else
        {
            unsupportedLayers.push_back(layer);
        }
    }

    for (const auto& layer : unsupportedLayers)
    {
        VEX_LOG(Warning, "Layer \"{}\" not supported", layer);
    }

    return supportedLayers;
}

} // namespace vex::vk
