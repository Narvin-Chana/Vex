#include "VkRHI.h"

#include <set>

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <RHI/RHICommandPool.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkCommandPool.h>
#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/RHI/VkGraphicsPipeline.h>
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

    ::vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    ::vk::ValidationFeaturesEXT validationFeatures;
    std::vector<::vk::ValidationFeatureEnableEXT> enables;

    if (enableGPUBasedValidation)
    {
        // Enable all validation layer features.
        enables = {
            ::vk::ValidationFeatureEnableEXT::eGpuAssisted,
            ::vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot,
            ::vk::ValidationFeatureEnableEXT::eDebugPrintf,
            ::vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
        };
        validationFeatures = {
            .enabledValidationFeatureCount = static_cast<u32>(enables.size()),
            .pEnabledValidationFeatures = enables.data(),
        };
    }
    if (enableGPUDebugLayer)
    {
        // Enable custom message callback.
        using Severity = ::vk::DebugUtilsMessageSeverityFlagBitsEXT;
        using MessageType = ::vk::DebugUtilsMessageTypeFlagBitsEXT;
        debugCreateInfo = {
            .pNext = enableGPUBasedValidation ? &validationFeatures : nullptr,
            .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eError,
            .messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
            .pfnUserCallback = debugCallback,
        };
    }

    std::vector<const char*> requiredInstanceExtensions = GetRequiredInstanceExtensions(enableGPUDebugLayer);

    std::vector<const char*> validationLayers = GetDefaultValidationLayers(enableGPUBasedValidation);
    validationLayers = FilterSupportedValidationLayers(validationLayers);

    ::vk::InstanceCreateInfo instanceCI{
        .sType = ::vk::StructureType::eInstanceCreateInfo,
        .pNext = enableGPUDebugLayer ? static_cast<void*>(&debugCreateInfo)
                                     : (enableGPUBasedValidation ? static_cast<void*>(&validationFeatures) : nullptr),
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
        bool presentSupported = VEX_VK_CHECK <<= physDevice.getSurfaceSupportKHR(i, *surface);

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
        featuresMutableDescriptors.pNext = &featuresAccelerationStructure.value();
    }

    // Allows for null descriptors
    ::vk::PhysicalDeviceRobustness2FeaturesEXT featuresRobustness;
    featuresRobustness.nullDescriptor = true;
    featuresRobustness.pNext = &featuresMutableDescriptors;

    ::vk::PhysicalDeviceVulkan13Features features13;
    features13.synchronization2 = true;
    features13.pNext = &featuresRobustness;
    features13.dynamicRendering = true;

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
    features12.bufferDeviceAddress = true;
    features12.vulkanMemoryModel = true;
    features12.vulkanMemoryModelDeviceScope = true;
    features12.storageBuffer8BitAccess = true;

    auto physDeviceFeatures = physDevice.getFeatures();
    ::vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &features12,
                                             .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                             .pQueueCreateInfos = queueCreateInfos.data(),
                                             .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                             .ppEnabledExtensionNames = extensions.data(),
                                             .pEnabledFeatures = &physDeviceFeatures };

    device = VEX_VK_CHECK <<= physDevice.createDeviceUnique(deviceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    auto createTimelineSemaphore = [&]()
    {
        ::vk::SemaphoreTypeCreateInfoKHR createInfo{ .semaphoreType = ::vk::SemaphoreType::eTimeline,
                                                     .initialValue = 0 };
        return VEX_VK_CHECK <<= device->createSemaphoreUnique(::vk::SemaphoreCreateInfo{
                   .pNext = &createInfo,
               });
    };

    if (graphicsQueueFamily == -1)
    {
        VEX_LOG(Fatal, "Unable to create graphics queue on device!");
    }
    commandQueues[CommandQueueTypes::Graphics] = VkCommandQueue{
        .type = CommandQueueTypes::Graphics,
        .family = static_cast<u32>(graphicsQueueFamily),
        .queue = device->getQueue(graphicsQueueFamily, 0),
        .nextSignalValue = 1,
        .timelineSemaphore = createTimelineSemaphore(),
    };

    SetDebugName(*device, device->getQueue(graphicsQueueFamily, 0), "Graphics Queue");

    if (computeQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Compute] = VkCommandQueue{
            .type = CommandQueueTypes::Compute,
            .family = static_cast<u32>(computeQueueFamily),
            .queue = device->getQueue(computeQueueFamily, 0),
            .nextSignalValue = 1,
            .timelineSemaphore = createTimelineSemaphore(),
        };
        SetDebugName(*device, device->getQueue(computeQueueFamily, 0), "Compute Queue");
    }

    if (copyQueueFamily != -1)
    {
        commandQueues[CommandQueueTypes::Copy] = VkCommandQueue{
            .type = CommandQueueTypes::Copy,
            .family = static_cast<u32>(copyQueueFamily),
            .queue = device->getQueue(copyQueueFamily, 0),
            .nextSignalValue = 1,
            .timelineSemaphore = createTimelineSemaphore(),
        };
        SetDebugName(*device, device->getQueue(copyQueueFamily, 0), "Copy Queue");
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

RHIGraphicsPipelineState VkRHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    return { key, *device, *PSOCache };
}

RHIComputePipelineState VkRHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return { key, *device, *PSOCache };
}

UniqueHandle<RHIResourceLayout> VkRHI::CreateResourceLayout(RHIDescriptorPool& descriptorPool)
{
    return MakeUnique<VkResourceLayout>(GetGPUContext(), descriptorPool);
}

RHITexture VkRHI::CreateTexture(const TextureDescription& description)
{
    return VkTexture(GetGPUContext(), TextureDescription(description));
}

RHIBuffer VkRHI::CreateBuffer(const BufferDescription& description)
{
    return VkBuffer(GetGPUContext(), description);
}

UniqueHandle<RHIDescriptorPool> VkRHI::CreateDescriptorPool()
{
    return MakeUnique<VkDescriptorPool>(GetGPUContext());
}

void VkRHI::ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args, std::vector<ShaderDefine>& defines)
{
    args.push_back(L"-spirv");
    args.push_back(L"-fvk-bind-resource-heap");
    args.push_back(L"0");
    args.push_back(L"0");
    defines.emplace_back(L"VEX_VULKAN");
}

void VkRHI::AcquireNextFrame(RHISwapChain& swapChain, u32 currentFrameIndex, RHITexture& currentBackbuffer)
{
    // Handle frames in flight - wait for this frame slot to be available
    auto& cmdQueue = commandQueues[CommandQueueType::Graphics];

    // Don't wait for the first N frames (no work has been queued yet).
    if (cmdQueue.nextSignalValue > std::to_underlying(swapChain.description.frameBuffering))
    {
        u64 waitValue = cmdQueue.nextSignalValue - std::to_underlying(swapChain.description.frameBuffering);
#if !VEX_SHIPPING
        // Helper for debugging issues with semaphores
        VEX_LOG(Verbose, "Waiting on semaphore value: {}", waitValue);
#endif

        // Wait until GPU has finished processing the frame that was using these resources previously (numFramesInFlight
        // frames ago)
        const ::vk::SemaphoreWaitInfo waitInfo = {
            .semaphoreCount = 1,
            .pSemaphores = &*cmdQueue.timelineSemaphore,
            .pValues = &waitValue,
        };
        VEX_VK_CHECK << device->waitSemaphores(&waitInfo, std::numeric_limits<u64>::max());
    }

    // Acquire the next image, potentially requiring a swapchain recreation.
    // TODO(https://trello.com/c/KVA9njlW): resizing logic is currently not stable at all. Requires further work on
    // fixing the RHI resize logic which currently is too DX-centric. Vk backbuffers can potentially change their
    // underlying image upon a resize, requiring us to modify the RHITexture of this backbuffer.
    auto res = device->acquireNextImageKHR(*swapChain.swapchain,
                                           std::numeric_limits<u64>::max(),
                                           *swapChain.backbufferAcquisition[currentFrameIndex],
                                           VK_NULL_HANDLE,
                                           &swapChain.currentBackbufferId);
    if (res == ::vk::Result::eErrorOutOfDateKHR)
    {
        swapChain.Resize(swapChain.width, swapChain.height);
        res = ::vk::Result::eSuccess;
    }

    VEX_VK_CHECK << res;

    // Transition to eUnknown
    currentBackbuffer.SetCurrentState(RHITextureState::Common);
}

void VkRHI::SubmitAndPresent(std::span<RHICommandList*> commandLists,
                             RHISwapChain& swapChain,
                             u32 currentFrameIndex,
                             bool isFullscreenMode)
{
    // Separate by queue type.
    std::vector<::vk::CommandBufferSubmitInfo> graphicsCmdLists, computeCmdLists, copyCmdLists;
    for (auto* cmdList : commandLists)
    {
        switch (cmdList->GetType())
        {
        case CommandQueueType::Graphics:
            graphicsCmdLists.push_back({
                .commandBuffer = *cmdList->commandBuffer,
                .deviceMask = 0,
            });
            break;
        case CommandQueueType::Compute:
            computeCmdLists.push_back({
                .commandBuffer = *cmdList->commandBuffer,
                .deviceMask = 0,
            });
            break;
        case CommandQueueType::Copy:
            copyCmdLists.push_back({
                .commandBuffer = *cmdList->commandBuffer,
                .deviceMask = 0,
            });
            break;
        }
    }

    // Complete all graphics queue rendering.
    if (!graphicsCmdLists.empty())
    {
        auto& cmdQueue = commandQueues[CommandQueueType::Graphics];
        u64 signalValue = cmdQueue.nextSignalValue++;

#if !VEX_SHIPPING
        VEX_LOG(Verbose, "About to signal timeline value: {}", signalValue);
#endif

#if !VEX_SHIPPING
        // Validate semaphore is in correct state
        if (auto preSubmitValue = VEX_VK_CHECK <<= device->getSemaphoreCounterValue(*cmdQueue.timelineSemaphore);
            preSubmitValue == std::numeric_limits<u64>::max())
        {
            VEX_LOG(Fatal, "Timeline semaphore is in error state (UINT64_MAX) before submission!");
            return;
        }
#endif
        // Before rendering the graphics queue, we must wait for acquireImage to be done.
        // This equates to waiting on this frame's backbufferAcquisition binary semaphore.
        ::vk::SemaphoreSubmitInfo acquireWaitInfo = {
            .semaphore = *swapChain.backbufferAcquisition[currentFrameIndex],
            .stageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };

        // Signal the timeline semaphore of this queue (allows for CPU-side waiting later on if the CPU moves too fast).
        ::vk::SemaphoreSubmitInfo timelineSignalInfo = {
            .semaphore = *cmdQueue.timelineSemaphore,
            .value = signalValue,
            .stageMask = ::vk::PipelineStageFlagBits2::eAllCommands,
        };
        // Signal the present binary semaphore (we only want to present once rendering work is done).
        ::vk::SemaphoreSubmitInfo presentSignalInfo = {
            .semaphore = *swapChain.presentSemaphore[currentFrameIndex],
            .stageMask = ::vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };
        std::array<::vk::SemaphoreSubmitInfo, 2> signalInfos = { timelineSignalInfo, presentSignalInfo };
        ::vk::SubmitInfo2 submitInfo = {
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &acquireWaitInfo,
            .commandBufferInfoCount = static_cast<u32>(graphicsCmdLists.size()),
            .pCommandBufferInfos = graphicsCmdLists.data(),
            .signalSemaphoreInfoCount = static_cast<u32>(signalInfos.size()),
            .pSignalSemaphoreInfos = signalInfos.data(),
        };

        VEX_VK_CHECK << cmdQueue.queue.submit2(submitInfo);
    }

    if (!computeCmdLists.empty())
    {
        // TODO(https://trello.com/c/4IEzErNL): Vex's multi-queue logic should be implemented in a more purposeful way.
        // This would require being able to take in user priorities and dependencies when a user creates a command
        // context on a specific queue. More thought has to be put into this, so its appropriate to be kept for later.
        VEX_NOT_YET_IMPLEMENTED();
    }

    if (!copyCmdLists.empty())
    {
        // TODO(https://trello.com/c/4IEzErNL): same as previous TODO.
        VEX_NOT_YET_IMPLEMENTED();
    }

    // Present.
    ::vk::PresentInfoKHR presentInfo = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*swapChain.presentSemaphore[currentFrameIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swapChain.swapchain,
        .pImageIndices = &swapChain.currentBackbufferId,
    };
    auto res = commandQueues[CommandQueueType::Graphics].queue.presentKHR(presentInfo);
    if (res == ::vk::Result::eErrorOutOfDateKHR)
    {
        swapChain.Resize(swapChain.width, swapChain.height);
        res = ::vk::Result::eSuccess;
    }

    VEX_VK_CHECK << res;
}

void VkRHI::FlushGPU()
{
    const auto& cmdQueue = commandQueues[CommandQueueType::Graphics];
    if (cmdQueue.nextSignalValue == 0)
    {
        // This means we're in the first frame, no work has been submitted yet!
        return;
    }

    // Force immediate queue flush
    VEX_VK_CHECK << cmdQueue.queue.waitIdle();

    // We want to wait for the most recently queued up signal (aka nextSignalValue - 1).
    const u64 waitValue = cmdQueue.nextSignalValue - 1;
    const ::vk::SemaphoreWaitInfo flushWaitInfo{
        .semaphoreCount = 1,
        .pSemaphores = &*cmdQueue.timelineSemaphore,
        .pValues = &waitValue,
    };
    VEX_VK_CHECK << device->waitSemaphores(&flushWaitInfo, std::numeric_limits<u64>::max());

    // TODO(https://trello.com/c/4IEzErNL): relating to the previous function's todos, we must be more mindful of
    // multi-queue work when we reach that point. This includes the flushing of the GPU (resolving dependencies).
}

NonNullPtr<VkGPUContext> VkRHI::GetGPUContext()
{
    static VkGPUContext ctx{ *device, physDevice, *surface, commandQueues[CommandQueueType::Graphics] };
    return NonNullPtr(ctx);
}

} // namespace vex::vk
