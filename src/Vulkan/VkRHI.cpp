#include "VkRHI.h"

#include <ranges>
#include <set>

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHI/RHICommandPool.h>
#include <Vex/RHI/RHIFence.h>

#include "VkCommandPool.h"
#include "VkCommandQueue.h"
#include "VkDebug.h"
#include "VkErrorHandler.h"
#include "VkExtensions.h"
#include "VkHeaders.h"
#include "VkPhysicalDevice.h"
#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

// Should be redone properly
static ::vk::PhysicalDeviceProperties GetHighestApiVersionDevice()
{
    // Create temporary instance to check device properties
    ::vk::UniqueInstance instance = CHECK <<= ::vk::createInstanceUnique({});
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    auto devices = CHECK <<= instance->enumeratePhysicalDevices();

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

    instance = CHECK <<= ::vk::createInstanceUnique(instanceCI);

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
    surface = CHECK <<= instance->createWin32SurfaceKHRUnique(createInfo);
#elif defined(__linux__)
    ::vk::XlibSurfaceCreateInfoKHR createInfo{
        .dpy = windowHandle.display,
        .window = windowHandle.window,
    };
    surface = CHECK <<= instance->createXlibSurfaceKHRUnique(createInfo);
#endif
}

std::vector<UniqueHandle<PhysicalDevice>> VkRHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<PhysicalDevice>> physicalDevices;

    std::vector<::vk::PhysicalDevice> vkPhysicalDevices = CHECK <<= instance->enumeratePhysicalDevices();
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

void VkRHI::Init(const UniqueHandle<PhysicalDevice>& vexPhysicalDevice)
{
    ::vk::PhysicalDevice physDevice = static_cast<VkPhysicalDevice*>(vexPhysicalDevice.get())->physicalDevice;

    i32 graphicsQueueFamily = -1;
    i32 computeQueueFamily = -1;
    i32 copyQueueFamily = -1;

    std::vector<::vk::QueueFamilyProperties> queueFamilies = physDevice.getQueueFamilyProperties();
    u32 i = 0;
    for (const auto& property : queueFamilies)
    {
        bool presentSupported = CHECK <<= physDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface);

        if (graphicsQueueFamily == -1 && presentSupported && property.queueFlags & ::vk::QueueFlagBits::eGraphics)
        {
            graphicsQueueFamily = static_cast<i32>(i);
        }
        else if (computeQueueFamily == -1 && property.queueFlags & ::vk::QueueFlagBits::eCompute)
        {
            computeQueueFamily = static_cast<i32>(i);
        }
        else if (copyQueueFamily == -1 && property.queueFlags & ::vk::QueueFlagBits::eTransfer)
        {
            copyQueueFamily = static_cast<i32>(i);
        }

        ++i;
    }

    std::set uniqueQueueFamilies{ graphicsQueueFamily, computeQueueFamily, copyQueueFamily };
    uniqueQueueFamilies.erase(-1);

    float queuePriority = 1.0f;
    std::vector<::vk::DeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        ::vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector extensions = GetDefaultDeviceExtensions();

    auto physDeviceFeatures = physDevice.getFeatures();

    ::vk::DeviceCreateInfo deviceCreateInfo{ .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                             .pQueueCreateInfos = queueCreateInfos.data(),
                                             .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                             .ppEnabledExtensionNames = extensions.data(),
                                             .pEnabledFeatures = &physDeviceFeatures };

    device = CHECK <<= physDevice.createDeviceUnique(deviceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    if (graphicsQueueFamily == -1)
    {
        VEX_LOG(Fatal, "Unable to create graphics queue on device!");
    }
    commandQueues[CommandQueueTypes::Graphics] = VkCommandQueue{ CommandQueueTypes::Graphics,
                                                                 static_cast<u32>(graphicsQueueFamily),
                                                                 device->getQueue(graphicsQueueFamily, 0) };

    if (computeQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Compute] = VkCommandQueue{ CommandQueueTypes::Compute,
                                                                    static_cast<u32>(computeQueueFamily),
                                                                    device->getQueue(computeQueueFamily, 0) };
    }

    if (copyQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Copy] = VkCommandQueue{ CommandQueueTypes::Copy,
                                                                 static_cast<u32>(copyQueueFamily),
                                                                 device->getQueue(copyQueueFamily, 0) };
    }
}

UniqueHandle<RHICommandPool> VkRHI::CreateCommandPool()
{
    return MakeUnique<VkCommandPool>(*device, commandQueues);
}

void VkRHI::ExecuteCommandList(RHICommandList& commandList)
{
    VkCommandList& cmdList = reinterpret_cast<VkCommandList&>(commandList);

    // TODO: make sure right semaphores are configured
    ::vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = 0,
        .commandBufferCount = 1,
        .pCommandBuffers = &*cmdList.commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    (void)commandQueues[cmdList.GetType()].queue.submit(submitInfo);
}

UniqueHandle<RHIFence> VkRHI::CreateFence(u32 numFenceIndices)
{
    return UniqueHandle<RHIFence>();
}

void VkRHI::SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
}

void VkRHI::WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
}

} // namespace vex::vk