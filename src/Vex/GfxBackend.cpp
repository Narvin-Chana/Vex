#include "GfxBackend.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <Vex/FeatureChecker.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHICommandList.h>
#include <Vex/RHI/RHICommandPool.h>
#include <Vex/RHI/RHIFence.h>
#include <Vex/RHI/RHISwapChain.h>

namespace vex
{

GfxBackend::GfxBackend(UniqueHandle<RHI>&& newRHI, const BackendDescription& description)
    : rhi(std::move(newRHI))
    , description(description)
    , commandPools(description.frameBuffering)
{
    VEX_LOG(Info, "Graphics API Support:\n\tDX12: {} Vulkan: {}", VEX_DX12, VEX_VULKAN);
    std::string vexTargetName;
    if (VEX_DEBUG)
    {
        vexTargetName = "Debug (no optimizations with debug symbols)";
    }
    else if (VEX_DEVELOPMENT)
    {
        vexTargetName = "Development (full optimizations with debug symbols)";
    }
    else if (VEX_SHIPPING)
    {
        vexTargetName = "Shipping (full optimizations with no debug symbols)";
    }
    VEX_LOG(Info, "Running Vex in {}", vexTargetName);

    auto physicalDevices = rhi->EnumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        VEX_LOG(Fatal, "The underlying graphics API was unable to find atleast one physical device.");
    }

    // Obtain the best physical device.
    std::sort(physicalDevices.begin(), physicalDevices.end(), [](const auto& l, const auto& r) { return *l > *r; });
#if !VEX_SHIPPING
    physicalDevices[0]->DumpPhysicalDeviceInfo();
#endif

    // Initializes RHI which includes creating logicial device and swapchain
    rhi->Init(physicalDevices[0]);

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);

    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        queueFrameFences[queueType] = rhi->CreateFence(std::to_underlying(description.frameBuffering));
    }

    for (auto frameIndex : magic_enum::enum_values<FrameBuffering>())
    {
        commandPools.Get(std::to_underlying(frameIndex) - 1) = rhi->CreateCommandPool();
    }

    swapChain = rhi->CreateSwapChain({ .format = description.swapChainFormat,
                                       .frameBuffering = description.frameBuffering,
                                       .useVSync = description.useVSync },
                                     description.platformWindow);
}

GfxBackend::~GfxBackend()
{
    // Wait for work to be done before starting the deletion of resources.
    FlushGPU();
}

void GfxBackend::StartFrame()
{
    // Forces a GPU flush on the third frame, to make sure everything works correctly :).
    static bool b = true;
    if (b && currentFrameIndex == 2)
    {
        FlushGPU();
        b = false;
    }

    VEX_LOG(Info, "Started Frame: {}", currentFrameIndex);

    // Test code creates two command lists on the graphics queue and one on compute.
    // They are opened then closed, then executed. Once the GPU is done with them the underlying memory is returned to
    // the pool.
    {
        auto cmdList = commandPools.Get(currentFrameIndex)->CreateCommandList(CommandQueueType::Graphics);
        cmdList->Open();
        cmdList->Close();

        rhi->ExecuteCommandList(*cmdList);
    }

    {
        auto cmdList = commandPools.Get(currentFrameIndex)->CreateCommandList(CommandQueueType::Compute);
        cmdList->Open();
        cmdList->Close();

        rhi->ExecuteCommandList(*cmdList);
    }

    {
        auto cmdList = commandPools.Get(currentFrameIndex)->CreateCommandList(CommandQueueType::Graphics);
        cmdList->Open();
        cmdList->Close();

        rhi->ExecuteCommandList(*cmdList);
    }
}

void GfxBackend::EndFrame()
{
    swapChain->Present();

    // Signal all queue fences.
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        rhi->SignalFence(queueType, *queueFrameFences[queueType], currentFrameIndex);
    }

    u32 nextFrameIndex = (currentFrameIndex + 1) % std::to_underlying(description.frameBuffering);

    // Wait for the fences nextFrameIndex value for all queue fences.
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        queueFrameFences[queueType]->WaitCPUAndIncrementNextFenceIndex(currentFrameIndex, nextFrameIndex);
    }

    currentFrameIndex = nextFrameIndex;

    // Release the memory occupied by the command lists that are done.
    commandPools.Get(currentFrameIndex)->ReclaimAllCommandListMemory();
}

void GfxBackend::FlushGPU()
{
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        // Increment fence values before signaling to ensure we're waiting on new values.
        ++queueFrameFences[queueType]->GetFenceValue(currentFrameIndex);
        // Signal fences with incremented value.
        rhi->SignalFence(queueType, *queueFrameFences[queueType], currentFrameIndex);
    }

    u32 nextFrameIndex = (currentFrameIndex + 1) % std::to_underlying(description.frameBuffering);

    // Wait for the currentFrameIndex we just signaled to be done for all queue fences.
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        queueFrameFences[queueType]->WaitCPUAndIncrementNextFenceIndex(currentFrameIndex, nextFrameIndex);
    }

    // The GPU should now be in an idle state.
    // Increment
    currentFrameIndex = nextFrameIndex;
}

void GfxBackend::SetVSync(bool useVSync)
{
    if (swapChain->NeedsFlushForVSyncToggle())
    {
        FlushGPU();
    }
    swapChain->SetVSync(useVSync);
}

} // namespace vex
