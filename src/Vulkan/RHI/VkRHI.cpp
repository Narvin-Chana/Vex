#include "VkRHI.h"

#include <set>
#include <utility>

#include <Vex/CommandContext.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/PlatformWindow.h>
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
#include <Vex/Synchronization.h>
#include <Vex/Types.h>
#include <Vex/Utility/Visitor.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkExtensions.h>
#include <Vulkan/VkHeaders.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vex::vk
{

struct DispatcherLifetime
{
    ::vk::Instance rhiInstance;
    ::vk::UniqueInstance tmpInstance;

    DispatcherLifetime()
    {
        VULKAN_HPP_DEFAULT_DISPATCHER = {};
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        tmpInstance = VEX_VK_CHECK <<= ::vk::createInstanceUnique({});
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*tmpInstance);
    }

    ::vk::Instance GetInstance()
    {
        if (rhiInstance)
        {
            return rhiInstance;
        }

        if (!tmpInstance)
        {
            tmpInstance = VEX_VK_CHECK <<= ::vk::createInstanceUnique({});
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*tmpInstance);
        }

        return *tmpInstance;
    }

    void SetInstance(::vk::Instance instance)
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
        rhiInstance = instance;
        tmpInstance.reset();
    }

    void SetDevice(::vk::Device device)
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    }
};

static DispatcherLifetime GDispatcherLifetime{};

VkRHI::VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation)
{
    // Reset global dispatcher, avoids potentially using stale pointers if a VulkanRHI was created previously.
    ::vk::ApplicationInfo appInfo{
        .pApplicationName = "Vex Vulkan App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vex Vulkan App",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    std::vector<const char*> layers;
    if (enableGPUBasedValidation)
    {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        layers.push_back("VK_LAYER_KHRONOS_synchronization2");
    }

    // Enumerate available extensions
    u32 extensionPropertiesCount;
    VEX_VK_CHECK << ::vk::enumerateInstanceExtensionProperties(nullptr, &extensionPropertiesCount, nullptr);
    std::vector<::vk::ExtensionProperties> extensionProperties(extensionPropertiesCount);
    VEX_VK_CHECK << ::vk::enumerateInstanceExtensionProperties(nullptr,
                                                               &extensionPropertiesCount,
                                                               extensionProperties.data());

#define VEX_VK_ADD_EXTENSION_CHECKED(extensionName)                                                                    \
    VEX_CHECK(SupportsExtension(extensionProperties, extensionName),                                                   \
              "Cannot create vk instance, unsupported extension: {}",                                                  \
              extensionName);                                                                                          \
    extensions.push_back(extensionName)

    // Enable extensions depending on what we require
    std::vector<const char*> extensions;

    const bool hasValidWindowHandle = !std::holds_alternative<std::monostate>(windowHandle.handle);
    // Some extensions are only needed if we have a valid window hande.
    if (hasValidWindowHandle)
    {
        // Required for any windowed application
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_SURFACE_EXTENSION_NAME);

        // Required for HDR swapchain handling
        VEX_VK_ADD_EXTENSION_CHECKED(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);

        // Platform-specific surface extensions
#if defined(_WIN32)
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#endif
    }

    if (enableGPUDebugLayer)
    {
        // Required for both resource debug names and debug message callbacks
        VEX_VK_ADD_EXTENSION_CHECKED(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

#undef VEX_VK_ADD_EXTENSION_CHECKED

    ::vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableGPUDebugLayer)
    {
        // Enable custom message callback.
        using Severity = ::vk::DebugUtilsMessageSeverityFlagBitsEXT;
        using MessageType = ::vk::DebugUtilsMessageTypeFlagBitsEXT;
        debugCreateInfo.messageSeverity = Severity::eVerbose | Severity::eInfo | Severity::eWarning | Severity::eError;
        debugCreateInfo.messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance;
        debugCreateInfo.pfnUserCallback = debugCallback;
    }

    ::vk::ValidationFeaturesEXT validationFeatures;
    static constexpr std::array enables = {
        ::vk::ValidationFeatureEnableEXT::eGpuAssisted,
        ::vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
    };
    if (enableGPUBasedValidation)
    {
        validationFeatures.setEnabledValidationFeatures(enables);
    }
    validationFeatures.pNext = &debugCreateInfo;

    ::vk::InstanceCreateInfo instanceCI{
        .pNext = &validationFeatures,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<u32>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<u32>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    instance = VEX_VK_CHECK <<= ::vk::createInstanceUnique(instanceCI);

    VEX_LOG(Info, "Created VK instance with layers:");
    for (auto validationLayer : layers)
    {
        VEX_LOG(Info, "\t{}", validationLayer);
    }

    VEX_LOG(Info, "Created VK instance with extensions:");
    for (auto instanceExtension : extensions)
    {
        VEX_LOG(Info, "\t{}", instanceExtension);
    }
    GDispatcherLifetime.SetInstance(*instance);

    // Only activate setting debug names if the debug layer is active. Otherwise Vulkan will error out.
    GEnableDebugName = enableGPUDebugLayer;

    if (hasValidWindowHandle)
    {
        InitWindow(windowHandle);
    }
}

VkRHI::~VkRHI() = default;

void VkRHI::InitWindow(const PlatformWindowHandle& platformWindowHandle)
{
    std::visit(
        Visitor{
#if defined(_WIN32)
            [this](const PlatformWindowHandle::WindowsHandle& windowHandle)
            {
                ::vk::Win32SurfaceCreateInfoKHR createInfo{
                    .hinstance = GetModuleHandle(nullptr),
                    .hwnd = windowHandle.window,
                };
                surface = VEX_VK_CHECK <<= instance->createWin32SurfaceKHRUnique(createInfo);
            },
#elif defined(__linux__)
            [this](const PlatformWindowHandle::X11Handle& windowHandle)
            {
                ::vk::XlibSurfaceCreateInfoKHR createInfo{
                    .dpy = windowHandle.display,
                    .window = windowHandle.window,
                };
                surface = VEX_VK_CHECK <<= instance->createXlibSurfaceKHRUnique(createInfo);
            },
            [this](const PlatformWindowHandle::WaylandHandle& windowHandle)
            {
                ::vk::WaylandSurfaceCreateInfoKHR createInfo{
                    .display = windowHandle.display,
                    .surface = windowHandle.window,
                };
                surface = VEX_VK_CHECK <<= instance->createWaylandSurfaceKHRUnique(createInfo);
            },
#endif
            [](auto&& args) {} },
        platformWindowHandle.handle);
}

std::vector<UniqueHandle<RHIPhysicalDevice>> VkRHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<RHIPhysicalDevice>> physicalDevices;

    ::vk::Instance instance = GDispatcherLifetime.GetInstance();

    std::vector<::vk::PhysicalDevice> vkPhysicalDevices = VEX_VK_CHECK <<= instance.enumeratePhysicalDevices();
    if (vkPhysicalDevices.empty())
    {
        VEX_LOG(Fatal, "No physical devices compatible with Vulkan were found!");
    }

    physicalDevices.reserve(vkPhysicalDevices.size());
    for (const ::vk::PhysicalDevice& dev : vkPhysicalDevices)
    {
        UniqueHandle<VkPhysicalDevice> newDevice = MakeUnique<VkPhysicalDevice>(dev);
        if (newDevice->SupportsMinimalRequirements())
        {
            physicalDevices.push_back(std::move(newDevice));
        }
    }

    return physicalDevices;
}

void VkRHI::Init()
{
    physDevice = GPhysicalDevice->physicalDevice;

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
    for (u32 queueFamily : uniqueQueueFamilies)
    {
        ::vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Enumerate available extensions
    u32 extensionPropertiesCount;
    VEX_VK_CHECK << physDevice.enumerateDeviceExtensionProperties(nullptr, &extensionPropertiesCount, nullptr);
    std::vector<::vk::ExtensionProperties> extensionProperties(extensionPropertiesCount);
    VEX_VK_CHECK << physDevice.enumerateDeviceExtensionProperties(nullptr,
                                                                  &extensionPropertiesCount,
                                                                  extensionProperties.data());

    std::vector<const char*> extensions;

#define VEX_VK_ADD_EXTENSION_CHECKED(extensionName)                                                                    \
    VEX_CHECK(SupportsExtension(extensionProperties, extensionName),                                                   \
              "Cannot create vk device, unsupported extension: {}",                                                    \
              extensionName);                                                                                          \
    extensions.push_back(extensionName)

    if (surface)
    {
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VEX_VK_ADD_EXTENSION_CHECKED(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_ROBUSTNESS_2_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_GOOGLE_USER_TYPE_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME);
    VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);

    if (GPhysicalDevice->IsFeatureSupported(Feature::RayTracing))
    {
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        VEX_VK_ADD_EXTENSION_CHECKED(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    }

#undef VEX_VK_ADD_EXTENSION_CHECKED

    // TODO(https://trello.com/c/rLevCOvT): vulkan ray tracing add required features
    ::vk::PhysicalDeviceAccelerationStructureFeaturesKHR featuresAccelerationStructure;

    ::vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR featuresUnifiedImageLayouts;
    featuresUnifiedImageLayouts.pNext = &featuresAccelerationStructure;
    featuresUnifiedImageLayouts.unifiedImageLayouts = true;

    // Allows for mutable descriptors
    ::vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT featuresMutableDescriptors;
    featuresMutableDescriptors.pNext = &featuresUnifiedImageLayouts;
    featuresMutableDescriptors.mutableDescriptorType = true;

    // Allows for the use of SV_Barycentrics in shaders.
    ::vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR featuresFragmentShaderBarycentric;
    featuresFragmentShaderBarycentric.pNext = &featuresMutableDescriptors;
    featuresFragmentShaderBarycentric.fragmentShaderBarycentric = true;

    // Allows for using derivatives in compute shaders.
    ::vk::PhysicalDeviceComputeShaderDerivativesFeaturesKHR featuresComputeShaderDerivatives;
    featuresComputeShaderDerivatives.pNext = &featuresFragmentShaderBarycentric;
    featuresComputeShaderDerivatives.computeDerivativeGroupQuads = true;
    featuresComputeShaderDerivatives.computeDerivativeGroupLinear = false;

    // Allows for null descriptors, robust access makes out of bounds accesses in shaders deterministic (return 0).
    // This better matches dx12 behavior.
    ::vk::PhysicalDeviceRobustness2FeaturesKHR featuresRobustness;
    featuresRobustness.pNext = &featuresComputeShaderDerivatives;
    featuresRobustness.robustBufferAccess2 = true;
    featuresRobustness.robustImageAccess2 = true;
    featuresRobustness.nullDescriptor = true;

    ::vk::PhysicalDeviceVulkan13Features features13;
    features13.pNext = &featuresRobustness;
    features13.synchronization2 = true;
    features13.dynamicRendering = true;
    // Allows for the use of "discard" in shaders.
    features13.shaderDemoteToHelperInvocation = true;

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
    features12.separateDepthStencilLayouts = true;

    ::vk::PhysicalDeviceVulkan11Features features11;
    features11.pNext = &features12;
    // Compatibility built-in shader variables for DX12: BaseInstance, BaseVertex and DrawIndex.
    // Required for certain HLSL/Slang SV_ intrinsics to work.
    features11.shaderDrawParameters = true;

    auto physDeviceFeatures = physDevice.getFeatures();
    // Geometry shader being enabled forces SV_PrimitiveID to also be enabled!
    // Without this, the semantic doesn't work in pixel shaders.
    physDeviceFeatures.geometryShader = true;

    ::vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &features11,
                                             .queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
                                             .pQueueCreateInfos = queueCreateInfos.data(),
                                             .enabledExtensionCount = static_cast<u32>(extensions.size()),
                                             .ppEnabledExtensionNames = extensions.data(),
                                             .pEnabledFeatures = &physDeviceFeatures };

    device = VEX_VK_CHECK <<= physDevice.createDeviceUnique(deviceCreateInfo);
    GDispatcherLifetime.SetDevice(*device);

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

RHISwapChain VkRHI::CreateSwapChain(SwapChainDesc& desc, const PlatformWindow& platformWindow)
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

RHIAccelerationStructure VkRHI::CreateAS(const ASDesc& desc)
{
    // TODO(https://trello.com/c/rLevCOvT): Implement vulkan AS upload/creation.
    VEX_NOT_YET_IMPLEMENTED();
    return VkAccelerationStructure(desc);
}

::vk::Instance VkRHI::GetNativeInstance()
{
    return *instance;
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
                               Span<const ::vk::CommandBufferSubmitInfo> commandBuffers,
                               Span<const ::vk::SemaphoreSubmitInfo> waitSemaphores,
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

VkRHI::DispatchRHILifetime::~DispatchRHILifetime()
{
    GDispatcherLifetime.SetInstance(nullptr);
}

std::vector<SyncToken> VkRHI::Submit(Span<const NonNullPtr<RHICommandList>> commandLists,
                                     Span<const SyncToken> dependencies)
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
