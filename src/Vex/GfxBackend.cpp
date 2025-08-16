#include "GfxBackend.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex.h>
#include <Vex/CommandContext.h>
#include <Vex/FeatureChecker.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHICommandPool.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RenderExtension.h>

namespace vex
{

GfxBackend::GfxBackend(const BackendDescription& description)
    : rhi(RHI(description.platformWindow.windowHandle,
              description.enableGPUDebugLayer,
              description.enableGPUBasedValidation))
    , description(description)
    , resourceCleanup(static_cast<i8>(description.frameBuffering))
    , commandPools(description.frameBuffering)
    , backBuffers(std::to_underlying(description.frameBuffering))
    , textureRegistry(DefaultRegistrySize)
    , bufferRegistry(DefaultRegistrySize)
{
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

    auto physicalDevices = rhi.EnumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        VEX_LOG(Fatal, "The underlying graphics API was unable to find atleast one physical device.");
    }

    // Obtain the best physical device.
    std::sort(physicalDevices.begin(), physicalDevices.end(), [](const auto& l, const auto& r) { return *l > *r; });

    if (GPhysicalDevice)
    {
        VEX_LOG(Fatal, "Cannot launch multiple instances of Vex...");
    }

    GPhysicalDevice = std::move(physicalDevices[0]);

#if !VEX_SHIPPING
    GPhysicalDevice->DumpPhysicalDeviceInfo();
#endif

    // Initializes RHI which includes creating logical device and swapchain
    rhi.Init(GPhysicalDevice);

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);

    commandPools.ForEach([&rhi = rhi](UniqueHandle<RHICommandPool>& el) { el = rhi.CreateCommandPool(); });

    descriptorPool = rhi.CreateDescriptorPool();

    psCache = PipelineStateCache(&rhi, *descriptorPool, &resourceCleanup, description.shaderCompilerSettings);

    swapChain = rhi.CreateSwapChain(
        {
            .format = description.swapChainFormat,
            .frameBuffering = description.frameBuffering,
            .useVSync = description.useVSync,
        },
        description.platformWindow);

    CreateBackBuffers();
}

GfxBackend::~GfxBackend()
{
    if (isInMiddleOfFrame)
    {
        VEX_LOG(Warning,
                "Destroying Vex GfxBackend in the middle of a frame, this is valid, although not recommended. Make "
                "sure each StartFrame has a matching EndFrame.");
        EndFrame(false);
        isInMiddleOfFrame = false;
    }
    // Wait for work to be done before starting the deletion of resources.
    FlushGPU();

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->Destroy();
    }

    // Clear the global physical device.
    GPhysicalDevice = nullptr;
}

void GfxBackend::StartFrame()
{
    rhi.AcquireNextFrame(*swapChain, currentFrameIndex, GetRHITexture(GetCurrentBackBuffer().handle));

    // Flush all resources that were queued up for deletion.
    resourceCleanup.FlushResources(1, *descriptorPool);

    // Release the memory occupied by the command lists that are done.
    GetCurrentCommandPool().ReclaimAllCommandListMemory();

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnFrameStart();
    }

    isInMiddleOfFrame = true;
}

void GfxBackend::EndFrame(bool isFullscreenMode)
{
    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnFrameEnd();
    }

    RHITexture& currentRHIBackBuffer = GetRHITexture(GetCurrentBackBuffer().handle);
    // Make sure the backbuffer is in Present mode, if not we will have to open a command list just to transition it.
    if (!(currentRHIBackBuffer.GetCurrentState() & RHITextureState::Present))
    {
        // We are forced to use a graphics queue when transitioning to the present state.
        RHICommandList* cmdList = commandPools.Get(currentFrameIndex)->CreateCommandList(CommandQueueType::Graphics);
        cmdList->Open();
        cmdList->Transition(currentRHIBackBuffer, RHITextureState::Present);
        cmdList->Close();
        queuedCommandLists.push_back(cmdList);
    }

    rhi.SubmitAndPresent(queuedCommandLists, *swapChain, currentFrameIndex, isFullscreenMode);
    queuedCommandLists.clear();

    currentFrameIndex = (currentFrameIndex + 1) % std::to_underlying(description.frameBuffering);

    // Send all shader errors to the user, only done on frame end, not on GPU flush.
    psCache.GetShaderCompiler().FlushCompilationErrors();

    ++frameCounter;

    isInMiddleOfFrame = false;
}

CommandContext GfxBackend::BeginScopedCommandContext(CommandQueueType queueType)
{
    return { this, GetCurrentCommandPool().CreateCommandList(queueType) };
}

void GfxBackend::EndCommandContext(RHICommandList& cmdList)
{
    // We want to close a command list asap, to allow for driver optimizations.
    cmdList.Close();

    // However the submitting of a commandList should be batched as much as possible for further driver optimizations
    // (allowed to append them together during execution or reorder if no dependencies exist).
    queuedCommandLists.push_back(&cmdList);
}

Texture GfxBackend::CreateTexture(TextureDescription description, ResourceLifetime lifetime)
{
    TextureUtil::ValidateTextureDescription(description);

    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO(https://trello.com/c/K2jgp9ax): handle dynamic resources, includes specifying that the resource when
        // bound should use dynamic bindless indices and self-cleanup of the RHITexture should occur after the current
        // frame ends, would be used for transient resources inside our memory allocation strategy (avoids constant
        // reallocations).
        VEX_NOT_YET_IMPLEMENTED();
    }

    return Texture{ .handle = textureRegistry.AllocateElement(std::move(rhi.CreateTexture(description))),
                    .description = std::move(description) };
}

Buffer GfxBackend::CreateBuffer(BufferDescription description, ResourceLifetime lifetime)
{
    BufferUtil::ValidateBufferDescription(description);

    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO(https://trello.com/c/K2jgp9ax): handle dynamic resources, includes specifying that the resource when
        // bound should use dynamic bindless indices and self-cleanup of the RHITexture should occur after the current
        // frame ends, would be used for transient resources inside our memory allocation strategy (avoids constant
        // reallocations).
        VEX_NOT_YET_IMPLEMENTED();
    }

    return Buffer{ .handle = bufferRegistry.AllocateElement(std::move(rhi.CreateBuffer(description))),
                   .description = std::move(description) };
}

void GfxBackend::UpdateData(const Buffer& buffer, std::span<const u8> data)
{
    VEX_ASSERT(data.size() <= buffer.description.byteSize, "Buffer data exceded buffer size");

    RHIBuffer& rhiBuffer = GetRHIBuffer(buffer.handle);
    rhiBuffer.GetMappedMemory().SetData(data);
}

void GfxBackend::DestroyTexture(const Texture& texture)
{
    resourceCleanup.CleanupResource(textureRegistry.ExtractElement(texture.handle));
}

void GfxBackend::DestroyBuffer(const Buffer& buffer)
{
    resourceCleanup.CleanupResource(bufferRegistry.ExtractElement(buffer.handle));
}

BindlessHandle GfxBackend::GetTextureBindlessHandle(const TextureBinding& bindlessResource, TextureUsage::Type usage)
{
    bindlessResource.Validate();

    auto& texture = GetRHITexture(bindlessResource.texture.handle);
    return texture.GetOrCreateBindlessView(bindlessResource, usage, *descriptorPool);
}

BindlessHandle GfxBackend::GetBufferBindlessHandle(const BufferBinding& bindlessResource)
{
    bindlessResource.Validate();

    auto& buffer = GetRHIBuffer(bindlessResource.buffer.handle);
    return buffer.GetOrCreateBindlessView(bindlessResource.usage, bindlessResource.stride, *descriptorPool);
}

void GfxBackend::FlushGPU()
{
    VEX_LOG(Info, "Forcing a GPU flush...");

    // In the case we're in the middle of a frame, we still want to submit all currently queued up work.
    if (isInMiddleOfFrame)
    {
        EndFrame(false);
    }

    rhi.FlushGPU();

    // Release all stale resource now that the GPU is done with them.
    resourceCleanup.FlushResources(std::to_underlying(description.frameBuffering), *descriptorPool);

    // Release the memory that is occupied by all our command lists.
    commandPools.ForEach([](auto& el) { el->ReclaimAllCommandListMemory(); });

    // Keep this transparent for the user, by starting up another frame automatically.
    if (isInMiddleOfFrame)
    {
        StartFrame();
        isInMiddleOfFrame = false;
    }

    VEX_LOG(Info, "GPU flush done.");
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

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnResize(newWidth, newHeight);
    }

    description.platformWindow.width = newWidth;
    description.platformWindow.height = newHeight;
}

Texture GfxBackend::GetCurrentBackBuffer()
{
    return backBuffers[currentFrameIndex];
}

void GfxBackend::RecompileAllShaders()
{
    if (description.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache.GetShaderCompiler().MarkAllShadersDirty();
    }
    else
    {
        VEX_LOG(Warning, "Cannot recompile shaders when not in shader debug mode.");
    }
}

void GfxBackend::SetShaderCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback)
{
    if (description.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache.GetShaderCompiler().SetCompilationErrorsCallback(callback);
    }
    else
    {
        VEX_LOG(Warning, "Cannot subscribe to shader errors when not in shader debug mode.");
    }
}

void GfxBackend::SetSamplers(std::span<TextureSampler> newSamplers)
{
    psCache.GetResourceLayout().SetSamplers(newSamplers);
}

RenderExtension* GfxBackend::RegisterRenderExtension(UniqueHandle<RenderExtension>&& renderExtension)
{
    renderExtension->data = RenderExtensionData{ .rhi = &rhi, .descriptorPool = descriptorPool.get() };
    renderExtension->Initialize();
    renderExtensions.push_back(std::move(renderExtension));
    return renderExtensions.back().get();
}

void GfxBackend::UnregisterRenderExtension(RenderExtension* renderExtension)
{
    auto el = std::ranges::find_if(renderExtensions,
                                   [renderExtension](const UniqueHandle<RenderExtension>& ext)
                                   { return ext.get() == renderExtension; });
    if (el != renderExtensions.end())
    {
        renderExtensions.erase(el);
    }
}

void GfxBackend::RecompileChangedShaders()
{
    if (description.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache.GetShaderCompiler().MarkAllStaleShadersDirty();
    }
    else
    {
        VEX_LOG(Warning, "Cannot recompile changed shaders when not in shader debug mode.");
    }
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
    return textureRegistry[textureHandle];
}

RHIBuffer& GfxBackend::GetRHIBuffer(BufferHandle bufferHandle)
{
    return bufferRegistry[bufferHandle];
}

void GfxBackend::CreateBackBuffers()
{
    backBuffers.resize(std::to_underlying(description.frameBuffering));
    for (u8 backBufferIndex = 0; backBufferIndex < std::to_underlying(description.frameBuffering); ++backBufferIndex)
    {
        RHITexture rhiTexture = swapChain->CreateBackBuffer(backBufferIndex);
        const auto& rhiDesc = rhiTexture.GetDescription();
        backBuffers[backBufferIndex] =
            Texture{ .handle = textureRegistry.AllocateElement(std::move(rhiTexture)), .description = rhiDesc };
    }

    // Recreating the backbuffers means resetting the current frame index to 0 (as if we've just started the application
    // from scratch).
    currentFrameIndex = 0;
}

} // namespace vex
