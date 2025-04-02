#include "VkRHI.h"

#include "Vex/Logger.h"
#include "Vex/PlatformWindow.h"
#include "VkDebug.h"
#include "VkErrorHandler.h"
#include "VkExtensions.h"
#include "VkHeaders.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

VkRHI::VkRHI(const RHICreateInfo& createInfo)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    std::vector<const char*> defaultInstanceExtensions = GetDefaultInstanceExtensions();
    std::ranges::copy(createInfo.additionnalExtensions, std::back_inserter(defaultInstanceExtensions));

    std::vector<const char*> validationLayers = GetDefaultValidationLayers();
    std::ranges::copy(createInfo.additionnalLayers, std::back_inserter(validationLayers));
    validationLayers = FilterSupportedValidationLayers(validationLayers);

    ::vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    ::vk::DebugUtilsMessengerCreateInfoEXT* debugCreateInfoPtr = nullptr;

#ifdef _DEBUG
    using Severity = ::vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = ::vk::DebugUtilsMessageTypeFlagBitsEXT;
    ::vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eError,
        .messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        .pfnUserCallback = debugCallback,
    };
    debugCreateInfoPtr = &debugCreateInfo;
#endif

    ::vk::InstanceCreateInfo instanceCI{
        .pNext = debugCreateInfoPtr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(defaultInstanceExtensions.size()),
        .ppEnabledExtensionNames = defaultInstanceExtensions.data(),
    };

    VEX_LOG(Info, "Create VK instances with layers:")
    for (auto validationLayer : validationLayers)
    {
        VEX_LOG(Info, "\t{}", validationLayer);
    }

    VEX_LOG(Info, "Create VK instances with extensions:")
    for (auto instanceExtension : defaultInstanceExtensions)
    {
        VEX_LOG(Info, "\t{}", instanceExtension);
    }

    instance = Sanitize(::vk::createInstanceUnique(instanceCI));

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
}

VkRHI::~VkRHI() = default;

void VkRHI::InitWindow(const PlatformWindow& window)
{
#if _WIN32
    ::vk::Win32SurfaceCreateInfoKHR createInfo{
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = window.windowHandle,
    };
    surface = Sanitize(instance->createWin32SurfaceKHRUnique(createInfo));
#elif defined(linux)
    ::vk::XlibSurfaceCreateInfoKHR createInfo{
        // TODO: configure linux stuff
    };
    surface = instance->createXlibSurfaceKHRUnique(createInfo);
#endif
}

FeatureChecker& VkRHI::GetFeatureChecker()
{
    return featureChecker;
}

} // namespace vex::vk