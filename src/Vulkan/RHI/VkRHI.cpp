#include "VkRHI.h"

#include <set>

#include <Vex/CommandContext.h>
#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHICommandPool.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHIFence.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RHIImpl/RHITimestampQueryPool.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Synchronization.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkExtensions.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkGraphicsPipeline.h>
#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkPhysicalDevice.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

// Should be redone properly. NC: ?? Should it?
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
    // Reset global dispatcher, avoids potentially using stale pointers if a VulkanRHI was created previously.
    VULKAN_HPP_DEFAULT_DISPATCHER = {};

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
        debugCreateInfo.pNext = enableGPUBasedValidation ? &validationFeatures : nullptr;
        debugCreateInfo.messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eError;
        debugCreateInfo.messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance;
        debugCreateInfo.pfnUserCallback = debugCallback;
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

    // Only activate setting debug names if the debug layer is active. Otherwise Vulkan will error out.
    GEnableDebugName = enableGPUDebugLayer;

    if (windowHandle.window)
    {
        InitWindow(windowHandle);
    }
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
        bool presentSupported = surface && (VEX_VK_CHECK <<= physDevice.getSurfaceSupportKHR(i, *surface));

        if (graphicsQueueFamily == -1 && (!surface || (surface && presentSupported)) &&
            property.queueFlags & ::vk::QueueFlagBits::eGraphics)
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
    features12.shaderSampledImageArrayNonUniformIndexing = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingUniformBufferUpdateAfterBind = true;
    features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingStorageImageUpdateAfterBind = true;
    features12.bufferDeviceAddress = true;
    features12.vulkanMemoryModel = true;
    features12.vulkanMemoryModelDeviceScope = true;
    features12.storageBuffer8BitAccess = true;
    features12.scalarBlockLayout = true;

    auto physDeviceFeatures = physDevice.getFeatures();
    ::vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &features12,
                                             .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                             .pQueueCreateInfos = queueCreateInfos.data(),
                                             .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                             .ppEnabledExtensionNames = extensions.data(),
                                             .pEnabledFeatures = &physDeviceFeatures };

    device = VEX_VK_CHECK <<= physDevice.createDeviceUnique(deviceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    if (graphicsQueueFamily != -1)
    {
        queues[QueueTypes::Graphics] = VkCommandQueue{
            .type = QueueTypes::Graphics,
            .family = static_cast<u32>(graphicsQueueFamily),
            .queue = device->getQueue(graphicsQueueFamily, 0),
        };
        SetDebugName(*device, device->getQueue(graphicsQueueFamily, 0), "Graphics Queue");
    }
    // When we have a surface we should have a graphics queue family
    else if (surface)
    {
        VEX_LOG(Fatal, "Unable to create graphics queue on device!");
    }

    if (computeQueueFamily != -1)
    {
        queues[QueueTypes::Compute] = VkCommandQueue{
            .type = QueueTypes::Compute,
            .family = static_cast<u32>(computeQueueFamily),
            .queue = device->getQueue(computeQueueFamily, 0),
        };
        SetDebugName(*device, device->getQueue(computeQueueFamily, 0), "Compute Queue");
    }

    if (copyQueueFamily != -1)
    {
        queues[QueueTypes::Copy] = VkCommandQueue{
            .type = QueueTypes::Copy,
            .family = static_cast<u32>(copyQueueFamily),
            .queue = device->getQueue(copyQueueFamily, 0),
        };
        SetDebugName(*device, device->getQueue(copyQueueFamily, 0), "Copy Queue");
    }

    fences = { VkFence(*device), VkFence(*device), VkFence(*device) };

    PSOCache = VEX_VK_CHECK <<= device->createPipelineCacheUnique({});

    // Initializes values for the first time
    GetGPUContext();
}

RHISwapChain VkRHI::CreateSwapChain(const SwapChainDescription& desc, const PlatformWindow& platformWindow)
{
    return { GetGPUContext(), desc, platformWindow };
}

RHICommandPool VkRHI::CreateCommandPool()
{
    return { *this, GetGPUContext(), queues };
}

RHIGraphicsPipelineState VkRHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    return { key, *device, *PSOCache };
}

RHIComputePipelineState VkRHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return { key, *device, *PSOCache };
}

RHIRayTracingPipelineState VkRHI::CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key)
{
    return { key, *device, *PSOCache };
}

RHIResourceLayout VkRHI::CreateResourceLayout(RHIDescriptorPool& descriptorPool)
{
    return { GetGPUContext(), descriptorPool };
}

RHITexture VkRHI::CreateTexture(RHIAllocator& allocator, const TextureDesc& desc)
{
    return { GetGPUContext(), allocator, TextureDesc(desc) };
}

RHIBuffer VkRHI::CreateBuffer(RHIAllocator& allocator, const BufferDesc& desc)
{
    return { GetGPUContext(), allocator, desc };
}

RHIDescriptorPool VkRHI::CreateDescriptorPool()
{
    return { GetGPUContext() };
}

RHIAllocator VkRHI::CreateAllocator()
{
    return { GetGPUContext() };
}

RHITimestampQueryPool VkRHI::CreateTimestampQueryPool(RHIAllocator& allocator)
{
    return { GetGPUContext(), *this, allocator };
}

void VkRHI::ModifyShaderCompilerEnvironment(ShaderCompilerBackend compilerBackend, ShaderEnvironment& shaderEnv)
{
    if (compilerBackend == ShaderCompilerBackend::DXC)
    {
        shaderEnv.args.emplace_back(L"-spirv");
        shaderEnv.args.emplace_back(L"-fvk-bind-resource-heap");
        shaderEnv.args.emplace_back(L"0");
        shaderEnv.args.emplace_back(L"1");
        shaderEnv.args.emplace_back(std::format(
            L"-fspv-target-env={}",
            StringToWString(std::string(reinterpret_cast<VkFeatureChecker&>(*GPhysicalDevice->featureChecker)
                                            .GetMaxSupportedVulkanVersion()))));
        shaderEnv.args.emplace_back(L"-fvk-use-dx-layout");
    }

    shaderEnv.defines.emplace_back("VEX_VULKAN");
}

void VkRHI::WaitForTokenOnCPU(const SyncToken& syncToken)
{
    auto& fence = (*fences)[syncToken.queueType];
    fence.WaitOnCPU(syncToken.value);
}

bool VkRHI::IsTokenComplete(const SyncToken& syncToken) const
{
    auto& fence = (*fences)[syncToken.queueType];
    return fence.GetValue() >= syncToken.value;
}

void VkRHI::WaitForTokenOnGPU(QueueType waitingQueue, const SyncToken& waitFor)
{
    pendingWaits[waitingQueue].push_back(waitFor);
}

std::array<SyncToken, QueueTypes::Count> VkRHI::GetMostRecentSyncTokenPerQueue() const
{
    std::array<SyncToken, QueueTypes::Count> highestSyncTokens;

    for (u8 i = 0; i < QueueTypes::Count; ++i)
    {
        highestSyncTokens[i] = { static_cast<QueueType>(i),
                                 ((*fences)[i].nextSignalValue != 0) * ((*fences)[i].nextSignalValue - 1) };
    }

    return highestSyncTokens;
}

void VkRHI::AddDependencyWait(std::vector<::vk::SemaphoreSubmitInfo>& waitSemaphores, SyncToken syncToken)
{
    auto& signalingFence = (*fences)[syncToken.queueType];
    waitSemaphores.push_back({ .semaphore = *signalingFence.timelineSemaphore,
                               .value = syncToken.value,
                               .stageMask = ::vk::PipelineStageFlagBits2::eAllCommands });
}

SyncToken VkRHI::SubmitToQueue(QueueType queueType,
                               std::span<::vk::CommandBufferSubmitInfo> commandBuffers,
                               std::span<::vk::SemaphoreSubmitInfo> waitSemaphores,
                               std::vector<::vk::SemaphoreSubmitInfo> signalSemaphores)
{
    auto& fence = (*fences)[queueType];
    auto& queue = queues[queueType];

    // Obtain signal value and increment for the next signal.
    u64 signalValue = fence.nextSignalValue++;

    ::vk::SemaphoreSubmitInfo signalInfo{ .semaphore = *fence.timelineSemaphore, .value = signalValue };
    signalSemaphores.push_back(signalInfo);

    ::vk::SubmitInfo2 submitInfo = {
        .waitSemaphoreInfoCount = static_cast<u32>(waitSemaphores.size()),
        .pWaitSemaphoreInfos = waitSemaphores.data(),
        .commandBufferInfoCount = static_cast<u32>(commandBuffers.size()),
        .pCommandBufferInfos = commandBuffers.data(),
        .signalSemaphoreInfoCount = static_cast<u32>(signalSemaphores.size()),
        .pSignalSemaphoreInfos = signalSemaphores.data(),
    };

    VEX_VK_CHECK << queue.queue.submit2(submitInfo);

    return SyncToken{ queueType, signalValue };
}

std::vector<SyncToken> VkRHI::Submit(std::span<NonNullPtr<RHICommandList>> commandLists,
                                     std::span<SyncToken> dependencies)
{
    // Group by queue type
    std::array<std::vector<::vk::CommandBufferSubmitInfo>, QueueTypes::Count> commandListsPerQueue;
    for (NonNullPtr<RHICommandList> cmdList : commandLists)
    {
        commandListsPerQueue[cmdList->GetType()].push_back(
            ::vk::CommandBufferSubmitInfo{ .commandBuffer = cmdList->GetNativeCommandList() });
    }

    std::vector<SyncToken> syncTokens;
    syncTokens.reserve(QueueTypes::Count);

    // Submit each queue separately
    for (u8 i = 0; i < QueueTypes::Count; ++i)
    {
        auto& cmdLists = commandListsPerQueue[i];
        if (cmdLists.empty())
            continue;

        QueueType queueType = static_cast<QueueType>(i);

        // Collect all waits for this queue
        std::vector<::vk::SemaphoreSubmitInfo> waitSemaphores;

        // Add explicit dependencies
        for (auto& dep : dependencies)
        {
            AddDependencyWait(waitSemaphores, dep);
        }

        // Add pending waits for this queue
        auto& pending = pendingWaits[i];
        for (auto& pendingWait : pending)
        {
            AddDependencyWait(waitSemaphores, pendingWait);
        }
        pending.clear();

        // Submit this queue's work
        SyncToken cmdListToken = SubmitToQueue(queueType, cmdLists, waitSemaphores);

        for (auto& cmdList : commandLists)
        {
            if (cmdList->GetType() == i)
            {
                cmdList->UpdateTimestampQueryTokens(cmdListToken);
            }
        }

        syncTokens.push_back(cmdListToken);
    }

    return syncTokens;
}

void VkRHI::FlushGPU()
{
    for (u8 i = 0; i < QueueTypes::Count; ++i)
    {
        QueueType queueType = static_cast<QueueType>(i);
        const auto& queue = queues[queueType];
        if (queue.type == QueueTypes::Invalid)
        {
            continue;
        }

        if (!queue.queue)
        {
            VEX_LOG(Warning, "VkQueue was invalid on flush, skipping flush operations on it")
            continue;
        }
        // Force immediate queue flush
        VEX_VK_CHECK << queue.queue.waitIdle();

        const auto& fence = (*fences)[queueType];
        // We want to wait for the most recently queued up signal (aka nextSignalValue - 1).
        const u64 waitValue = fence.nextSignalValue - 1;
        const ::vk::SemaphoreWaitInfo flushWaitInfo{
            .semaphoreCount = 1,
            .pSemaphores = &*fence.timelineSemaphore,
            .pValues = &waitValue,
        };
        VEX_VK_CHECK << device->waitSemaphores(&flushWaitInfo, std::numeric_limits<u64>::max());
    }
}

NonNullPtr<VkGPUContext> VkRHI::GetGPUContext()
{
    if (!ctx)
    {
        ctx = MakeUnique<VkGPUContext>(*device,
                                       physDevice,
                                       *surface,
                                       queues[QueueType::Graphics],
                                       (*fences)[QueueType::Graphics]);
    }
    return NonNullPtr(*ctx);
}

} // namespace vex::vk
