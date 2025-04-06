#include "VkRHI.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include "VkDebug.h"
#include "VkErrorHandler.h"
#include "VkExtensions.h"
#include "VkHeaders.h"
#include "VkPhysicalDevice.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

// Should be redone properly
static ::vk::PhysicalDeviceProperties GetHighestApiVersionDevice()
{
    ::vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    // Create temporary instance to check device properties
    ::vk::UniqueInstance instance = Sanitize(::vk::createInstanceUnique({
        .pApplicationInfo = &appInfo,
    }));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    auto devices = Sanitize(instance->enumeratePhysicalDevices());

    ::vk::PhysicalDeviceProperties bestDevice{};
    for (auto dev : devices)
    {
        auto prop = dev.getProperties();
        if (prop.apiVersion > bestDevice.apiVersion)
        {
            bestDevice = prop;
        }
    }

    return bestDevice;
}

VkRHI::VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    ::vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = GetHighestApiVersionDevice().apiVersion,
    };

    ::vk::DebugUtilsMessengerCreateInfoEXT* debugCreateInfoPtr = nullptr;

    if (enableGPUDebugLayer)
    {
        using Severity = ::vk::DebugUtilsMessageSeverityFlagBitsEXT;
        using MessageType = ::vk::DebugUtilsMessageTypeFlagBitsEXT;
        ::vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
            .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eError,
            .messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
            .pfnUserCallback = debugCallback,
        };
        debugCreateInfoPtr = &debugCreateInfo;
    }

    std::vector<const char*> requiredInstanceExtensions = GetRequiredInstanceExtensions(enableGPUDebugLayer);

    std::vector<const char*> validationLayers = GetDefaultValidationLayers(enableGPUBasedValidation);
    validationLayers = FilterSupportedValidationLayers(validationLayers);

    ::vk::InstanceCreateInfo instanceCI{
        .pNext = debugCreateInfoPtr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size()),
        .ppEnabledExtensionNames = requiredInstanceExtensions.data(),
    };

    VEX_LOG(Info, "Create VK instances with layers:")
    for (auto validationLayer : validationLayers)
    {
        VEX_LOG(Info, "\t{}", validationLayer);
    }

    VEX_LOG(Info, "Create VK instances with extensions:")
    for (auto instanceExtension : requiredInstanceExtensions)
    {
        VEX_LOG(Info, "\t{}", instanceExtension);
    }

    instance = Sanitize(::vk::createInstanceUnique(instanceCI));

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    InitWindow(windowHandle);
}

VkRHI::~VkRHI() = default;

void VkRHI::InitWindow(const PlatformWindowHandle& windowHandle)
{
#if defined(_WIN32)
    ::vk::Win32SurfaceCreateInfoKHR createInfo{
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = windowHandle.window,
    };
    surface = Sanitize(instance->createWin32SurfaceKHRUnique(createInfo));
#elif defined(__linux__)
    ::vk::XlibSurfaceCreateInfoKHR createInfo{
        .dpy = windowHandle.display,
        .window = windowHandle.window,
    };
    surface = Sanitize(instance->createXlibSurfaceKHRUnique(createInfo));
#endif
}

std::vector<UniqueHandle<PhysicalDevice>> VkRHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<PhysicalDevice>> physicalDevices;

    std::vector<::vk::PhysicalDevice> vkPhysicalDevices = Sanitize(instance->enumeratePhysicalDevices());
    if (vkPhysicalDevices.empty())
    {
        VEX_LOG(Fatal, "No physical devices compatible with Vulkan were found!");
    }

    physicalDevices.reserve(vkPhysicalDevices.size());
    for (const ::vk::PhysicalDevice& dev : vkPhysicalDevices)
    {
        physicalDevices.push_back(MakeUnique<vex::vk::VkPhysicalDevice>(dev));
    }

    return physicalDevices;
}

void VkRHI::Init(const UniqueHandle<PhysicalDevice>& physicalDevice)
{
    // TODO: init device for the passed in physical device.
}

} // namespace vex::vk