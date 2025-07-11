#include "GfxBackend.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <Vex/CommandContext.h>
#include <Vex/FeatureChecker.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHICommandList.h>
#include <Vex/RHI/RHICommandPool.h>
#include <Vex/RHI/RHIDescriptorPool.h>
#include <Vex/RHI/RHIFence.h>
#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/RHI/RHIShader.h>
#include <Vex/RHI/RHISwapChain.h>
#include <Vex/RHI/RHITexture.h>

namespace vex
{

GfxBackend::GfxBackend(UniqueHandle<RHI>&& newRHI, const BackendDescription& description)
    : rhi(std::move(newRHI))
    , description(description)
    , resourceCleanup(static_cast<i8>(description.frameBuffering))
    , commandPools(description.frameBuffering)
    , backBuffers(std::to_underlying(description.frameBuffering))
    , textureRegistry(DefaultRegistrySize)
    , bufferRegistry(DefaultRegistrySize)
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

    physicalDevice = std::move(physicalDevices[0]);

#if !VEX_SHIPPING
    physicalDevice->DumpPhysicalDeviceInfo();
#endif

    // Initializes RHI which includes creating logicial device and swapchain
    rhi->Init(physicalDevice);

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);

    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        queueFrameFences[queueType] = rhi->CreateFence(std::to_underlying(description.frameBuffering));
    }

    commandPools.ForEach([this](UniqueHandle<RHICommandPool>& el) { el = rhi->CreateCommandPool(); });

    descriptorPool = rhi->CreateDescriptorPool();

    psCache = PipelineStateCache(rhi.get(),
                                 *descriptorPool,
                                 *physicalDevice->featureChecker,
                                 &resourceCleanup,
                                 description.enableShaderDebugging);

    swapChain = rhi->CreateSwapChain({ .format = description.swapChainFormat,
                                       .frameBuffering = description.frameBuffering,
                                       .useVSync = description.useVSync },
                                     description.platformWindow);

    CreateBackBuffers();
}

GfxBackend::~GfxBackend()
{
    // Wait for work to be done before starting the deletion of resources.
    FlushGPU();
}

void GfxBackend::StartFrame()
{
    swapChain->AcquireNextBackbuffer(currentFrameIndex);
}

void GfxBackend::EndFrame(bool isFullscreenMode)
{
    swapChain->Present(isFullscreenMode);

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

    // Flush all resources that were queued up for deletion.
    resourceCleanup.FlushResources(1, *descriptorPool);

    // Release the memory occupied by the command lists that are done.
    GetCurrentCommandPool().ReclaimAllCommandListMemory();
}

CommandContext GfxBackend::BeginScopedCommandContext(CommandQueueType queueType)
{
    return { this, GetCurrentCommandPool().CreateCommandList(queueType) };
}

void GfxBackend::EndCommandContext(RHICommandList& cmdList)
{
    // Force all backbuffers back to the present mode, this is because they could have been manipulated during the
    // context's execution and thus changed state. There might be a better way to handle this....
    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> transitions;
    transitions.reserve(backBuffers.size());
    for (auto& bb : backBuffers)
    {
        transitions.emplace_back(GetRHITexture(bb.handle), RHITextureState::Present);
    }
    cmdList.Transition(transitions);

    cmdList.Close();
    rhi->ExecuteCommandList(cmdList);
}

Texture GfxBackend::CreateTexture(TextureDescription description, ResourceLifetime lifetime)
{
    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO: handle dynamic resources, includes specifying that the resource when bound should use dynamic bindless
        // indices and self-cleanup of the RHITexture should occur after the current frame ends.
        VEX_NOT_YET_IMPLEMENTED();
    }

    auto rhiTexture = rhi->CreateTexture(description);
    return Texture{ .handle = textureRegistry.AllocateElement(std::move(rhiTexture)),
                    .description = std::move(description) };
}

void GfxBackend::DestroyTexture(const Texture& texture)
{
    resourceCleanup.CleanupResource(std::move(textureRegistry[texture.handle]));
    textureRegistry.FreeElement(texture.handle);
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

    // Wait for the currentFrameIndex we just signaled to be done for all queue fences.
    for (auto queueType : magic_enum::enum_values<CommandQueueType>())
    {
        queueFrameFences[queueType]->WaitCPU(currentFrameIndex);
    }

    // Release all stale resource now that the GPU is done with them.
    resourceCleanup.FlushResources(std::to_underlying(description.frameBuffering), *descriptorPool);

    // Release the memory occupied by the command lists that are done.
    commandPools.ForEach([](auto& el) { el->ReclaimAllCommandListMemory(); });
}

void GfxBackend::SetVSync(bool useVSync)
{
    if (swapChain->NeedsFlushForVSyncToggle())
    {
        FlushGPU();
    }
    swapChain->SetVSync(useVSync);
}

void GfxBackend::OnWindowResized(u32 newWidth, u32 newHeight)
{
    // Do not resize if any of the dimensions is 0, or if the resize gives us the same window size as we have
    // currently.
    if (newWidth == 0 || newHeight == 0 ||
        (newWidth == description.platformWindow.width && newHeight == description.platformWindow.height))
    {
        return;
    }

    FlushGPU();

    // No need to handle texture lifespan since we've flushed the GPU.
    for (auto& backBuffer : backBuffers)
    {
        textureRegistry.FreeElement(backBuffer.handle);
    }
    backBuffers.clear();
    swapChain->Resize(newWidth, newHeight);
    CreateBackBuffers();

    description.platformWindow.width = newWidth;
    description.platformWindow.height = newHeight;
}

Texture GfxBackend::GetCurrentBackBuffer()
{
    return backBuffers[currentFrameIndex];
}

void GfxBackend::RecompileAllShaders()
{
    psCache.GetShaderCache().MarkAllShadersDirty();
}

void GfxBackend::RecompileChangedShaders()
{
    psCache.GetShaderCache().MarkAllStaleShadersDirty();
}

PipelineStateCache& GfxBackend::GetPipelineStateCache()
{
    return psCache;
}

RHICommandPool& GfxBackend::GetCurrentCommandPool()
{
    return *commandPools.Get(currentFrameIndex);
}

RHITexture& GfxBackend::GetRHITexture(TextureHandle textureHandle)
{
    return *textureRegistry[textureHandle];
}

RHIBuffer& GfxBackend::GetRHIBuffer(BufferHandle bufferHandle)
{
    return *bufferRegistry[bufferHandle];
}

void GfxBackend::CreateBackBuffers()
{
    backBuffers.resize(std::to_underlying(description.frameBuffering));
    for (u8 backBufferIndex = 0; backBufferIndex < std::to_underlying(description.frameBuffering); ++backBufferIndex)
    {
        auto rhiTexture = swapChain->CreateBackBuffer(backBufferIndex);
        const auto& rhiDesc = rhiTexture->GetDescription();
        backBuffers[backBufferIndex] =
            Texture{ .handle = textureRegistry.AllocateElement(std::move(rhiTexture)), .description = rhiDesc };
    }

    // Recreating the backbuffers means resetting the current frame index to 0 (as if we've just started the application
    // from scratch).
    currentFrameIndex = 0;
}

} // namespace vex
