#include "VkRHI.h"

#include <ranges>
#include <set>

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <RHI/RHICommandPool.h>
#include <RHI/RHIFence.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkCommandPool.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkFence.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkSwapChain.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkExtensions.h>
#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkPhysicalDevice.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

// Should be redone properly
static ::vk::PhysicalDeviceProperties GetHighestApiVersionDevice()
{
    // Create temporary instance to check device properties
    ::vk::UniqueInstance instance = VEX_VK_CHECK <<= ::vk::createInstanceUnique({});
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    auto devices = VEX_VK_CHECK <<= instance->enumeratePhysicalDevices();

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
        .sType = ::vk::StructureType::eInstanceCreateInfo,
        .pNext = debugCreateInfoPtr,
        .flags = {},
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<u32>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = static_cast<u32>(requiredInstanceExtensions.size()),
        .ppEnabledExtensionNames = requiredInstanceExtensions.data(),
    };

    VEX_LOG(Info, "Create VK instances with layers:");
    for (auto validationLayer : validationLayers)
    {
        VEX_LOG(Info, "\t{}", validationLayer);
    }

    VEX_LOG(Info, "Create VK instances with extensions:");
    for (auto instanceExtension : requiredInstanceExtensions)
    {
        VEX_LOG(Info, "\t{}", instanceExtension);
    }

    instance = VEX_VK_CHECK <<= ::vk::createInstanceUnique(instanceCI);

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
    surface = VEX_VK_CHECK <<= instance->createWin32SurfaceKHRUnique(createInfo);
#elif defined(__linux__)
    ::vk::XlibSurfaceCreateInfoKHR createInfo{
        .dpy = windowHandle.display,
        .window = windowHandle.window,
    };
    surface = VEX_VK_CHECK <<= instance->createXlibSurfaceKHRUnique(createInfo);
#endif
}

std::vector<UniqueHandle<PhysicalDevice>> VkRHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<PhysicalDevice>> physicalDevices;

    std::vector<::vk::PhysicalDevice> vkPhysicalDevices = VEX_VK_CHECK <<= instance->enumeratePhysicalDevices();
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
    physDevice = static_cast<VkPhysicalDevice*>(vexPhysicalDevice.get())->physicalDevice;

    i32 graphicsQueueFamily = -1;
    i32 computeQueueFamily = -1;
    i32 copyQueueFamily = -1;

    std::vector<::vk::QueueFamilyProperties> queueFamilies = physDevice.getQueueFamilyProperties();
    u32 i = 0;
    for (const auto& property : queueFamilies)
    {
        bool presentSupported = VEX_VK_CHECK <<= physDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface);

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

    std::optional<::vk::PhysicalDeviceAccelerationStructureFeaturesKHR> featuresAccelerationStructure;
    if (vexPhysicalDevice->featureChecker->IsFeatureSupported(Feature::RayTracing))
    {
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        featuresAccelerationStructure = {
            .accelerationStructure = true,
            .descriptorBindingAccelerationStructureUpdateAfterBind = true,
        };
    }

    // Allows for mutable descriptors
    ::vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT featuresMutableDescriptors;
    featuresMutableDescriptors.mutableDescriptorType = true;
    if (featuresAccelerationStructure.has_value())
    {
        featuresMutableDescriptors.pNext = featuresAccelerationStructure.value();
    }

    // Allows for null descriptors
    ::vk::PhysicalDeviceRobustness2FeaturesEXT featuresRobustness;
    featuresRobustness.nullDescriptor = true;
    featuresRobustness.pNext = featuresMutableDescriptors;

    ::vk::PhysicalDeviceVulkan13Features features13;
    features13.synchronization2 = true;
    features13.pNext = featuresRobustness;

    ::vk::PhysicalDeviceVulkan12Features features12;
    features12.pNext = &features13;
    features12.timelineSemaphore = true;
    features12.descriptorIndexing = true;

    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingUniformBufferUpdateAfterBind = true;
    features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingStorageImageUpdateAfterBind = true;

    auto physDeviceFeatures = physDevice.getFeatures();
    ::vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &features12,
                                             .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                             .pQueueCreateInfos = queueCreateInfos.data(),
                                             .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                             .ppEnabledExtensionNames = extensions.data(),
                                             .pEnabledFeatures = &physDeviceFeatures };

    device = VEX_VK_CHECK <<= physDevice.createDeviceUnique(deviceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    auto createSemaphore = [&]
    {
        ::vk::SemaphoreTypeCreateInfoKHR type{ .semaphoreType = ::vk::SemaphoreType::eTimeline, .initialValue = 0 };

        return VEX_VK_CHECK <<= device->createSemaphoreUnique(::vk::SemaphoreCreateInfo{
                   .pNext = &type,
               });
    };

    if (graphicsQueueFamily == -1)
    {
        VEX_LOG(Fatal, "Unable to create graphics queue on device!");
    }
    commandQueues[CommandQueueTypes::Graphics] = VkCommandQueue{ CommandQueueTypes::Graphics,
                                                                 static_cast<u32>(graphicsQueueFamily),
                                                                 device->getQueue(graphicsQueueFamily, 0),
                                                                 0,
                                                                 createSemaphore() };

    if (computeQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Compute] = VkCommandQueue{ CommandQueueTypes::Compute,
                                                                    static_cast<u32>(computeQueueFamily),
                                                                    device->getQueue(computeQueueFamily, 0),
                                                                    0,
                                                                    createSemaphore() };
    }

    if (copyQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Copy] = VkCommandQueue{ CommandQueueTypes::Copy,
                                                                 static_cast<u32>(copyQueueFamily),
                                                                 device->getQueue(copyQueueFamily, 0),
                                                                 0,
                                                                 createSemaphore() };
    }

    PSOCache = VEX_VK_CHECK <<= device->createPipelineCacheUnique({});

    // Initializes values for the first time
    GetGPUContext();
}

UniqueHandle<RHISwapChain> VkRHI::CreateSwapChain(const SwapChainDescription& description,
                                                  const PlatformWindow& platformWindow)
{
    return MakeUnique<VkSwapChain>(GetGPUContext(), description, platformWindow);
}

UniqueHandle<RHICommandPool> VkRHI::CreateCommandPool()
{
    return MakeUnique<VkCommandPool>(GetGPUContext(), commandQueues);
}

UniqueHandle<RHIGraphicsPipelineState> VkRHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    return MakeUnique<VkGraphicsPipelineState>(key);
}

UniqueHandle<RHIComputePipelineState> VkRHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return MakeUnique<VkComputePipelineState>(key, *device, *PSOCache);
}

UniqueHandle<RHIResourceLayout> VkRHI::CreateResourceLayout(RHIDescriptorPool& descriptorPool)
{
    return MakeUnique<VkResourceLayout>(*device, descriptorPool);
}

UniqueHandle<RHITexture> VkRHI::CreateTexture(const TextureDescription& description)
{
    return MakeUnique<VkImageTexture>(GetGPUContext(), TextureDescription(description));
}

UniqueHandle<RHIBuffer> VkRHI::CreateBuffer(const BufferDescription& description)
{
    return MakeUnique<VkBuffer>(GetGPUContext(), description);
}

UniqueHandle<RHIDescriptorPool> VkRHI::CreateDescriptorPool()
{
    return MakeUnique<VkDescriptorPool>(*device);
}

void VkRHI::ExecuteCommandList(RHICommandList& commandList)
{
    auto& cmdQueue = commandQueues[commandList.GetType()];

    ::vk::CommandBufferSubmitInfoKHR cmdBuffreInfo{
        .commandBuffer = *commandList.commandBuffer,
        .deviceMask = 0,
    };

    ::vk::SemaphoreSubmitInfo semWaitInfo{
        .semaphore = *cmdQueue.waitSemaphore,
        .value = cmdQueue.waitValue,
    };

    ::vk::SemaphoreSubmitInfo semSignalInfo{
        .semaphore = *cmdQueue.waitSemaphore,
        .value = ++cmdQueue.waitValue,
    };

    ::vk::SubmitInfo2KHR submitInfo{ .waitSemaphoreInfoCount = 1,
                                     .pWaitSemaphoreInfos = &semWaitInfo,
                                     .commandBufferInfoCount = 1,
                                     .pCommandBufferInfos = &cmdBuffreInfo,
                                     .signalSemaphoreInfoCount = 1,
                                     .pSignalSemaphoreInfos = &semSignalInfo };

    VEX_VK_CHECK << cmdQueue.queue.submit2KHR(submitInfo);
}

void VkRHI::ExecuteCommandLists(std::span<RHICommandList*> commandLists)
{
    // TODO(https://trello.com/c/J4WjMkVk): Improve this, the other function has semaphores and stuff, it seemed a bit
    // complicated, so I just call it once per CommandList for now (which is obviously not optimal).
    for (RHICommandList* cmdList : commandLists)
    {
        ExecuteCommandList(*cmdList);
    }
}

UniqueHandle<RHIFence> VkRHI::CreateFence(u32 numFenceIndices)
{
    return MakeUnique<VkFence>(numFenceIndices, *device);
}

void VkRHI::SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    ::vk::TimelineSemaphoreSubmitInfoKHR timelineInfo{ .signalSemaphoreValueCount = 1,
                                                       .pSignalSemaphoreValues = &fence.GetFenceValue(fenceIndex) };

    ::vk::SubmitInfo submit{
        .pNext = &timelineInfo,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*fence.fence,
    };

    VEX_VK_CHECK << commandQueues[queueType].queue.submit(submit);
}

void VkRHI::WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    VkTimelineSemaphoreSubmitInfoKHR timelineInfo;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &fence.GetFenceValue(fenceIndex);

    ::vk::SubmitInfo submit;
    submit.pNext = &timelineInfo;
    submit.pWaitSemaphores = &*fence.fence;
    submit.waitSemaphoreCount = 1;

    VEX_VK_CHECK << commandQueues[queueType].queue.submit(submit);
}

void VkRHI::ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args, std::vector<ShaderDefine>& defines)
{
    args.push_back(L"-spirv");
    args.push_back(L"-fvk-bind-resource-heap");
    args.push_back(L"0");
    args.push_back(L"0");
    defines.emplace_back(L"VEX_VULKAN");
}

VkGPUContext& VkRHI::GetGPUContext()
{
    static VkGPUContext ctx{ *device, physDevice, *surface, commandQueues[CommandQueueType::Graphics] };
    return ctx;
}

} // namespace vex::vk
