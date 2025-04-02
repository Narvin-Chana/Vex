#include "VkExtensions.h"
#include "Vex/Logger.h"
#include "VkHeaders.h"

#include <ranges>
#include <unordered_set>
#include <vulkan/vulkan_core.h>

namespace vex::vk
{

std::vector<const char*> GetDefaultInstanceExtensions()
{
    return {
#ifdef _DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };
}

std::vector<const char*> GetDefaultValidationLayers()
{
    return {
#ifdef _DEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };
}

std::vector<const char*> GetDefaultDeviceExtensions()
{
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
}

std::vector<const char*> FilterSupportedValidationLayers(const std::vector<const char*>& layers)
{
    uint32_t layerCount;
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
