#include "GfxBackend.h"

#include <utility>

#include <magic_enum/magic_enum.hpp>

#include <Vex.h>
#include <Vex/ByteUtils.h>
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
    , resourceCleanup(rhi)
    , textureRegistry(DefaultRegistrySize)
    , bufferRegistry(DefaultRegistrySize)
    , presentTextures(std::to_underlying(description.frameBuffering))
    , presentTokens(std::to_underlying(description.frameBuffering))
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

    // Initializes RHI which includes creating logical device and swapchain.
    rhi.Init(GPhysicalDevice);

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);

    commandPool.emplace(rhi.CreateCommandPool());

    descriptorPool = rhi.CreateDescriptorPool();

    psCache.emplace(&rhi, *descriptorPool, &resourceCleanup, description.shaderCompilerSettings);

    allocator = rhi.CreateAllocator();

    if (description.useSwapChain)
    {
        swapChain.emplace(rhi.CreateSwapChain(
            {
                .format = description.swapChainFormat,
                .frameBuffering = description.frameBuffering,
                .useVSync = description.useVSync,
            },
            description.platformWindow));

        CreatePresentTextures();
    }
}

GfxBackend::~GfxBackend()
{
    if (!deferredSubmissionCommandLists.empty())
    {
        VEX_LOG(Warning,
                "Destroying Vex GfxBackend in the middle of a frame, this is valid, although not recommended."
                "Make sure to not exit before Presenting if you use the Deferred submission policy as otherwise this "
                "could result in uncompleted work.");
    }

    //  Wait for work to be done before starting the deletion of resources.
    FlushGPU();

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->Destroy();
    }

    // Clear the global physical device.
    GPhysicalDevice = nullptr;
}

void GfxBackend::Present(bool isFullscreenMode)
{
    if (!description.useSwapChain)
    {
        VEX_LOG(Fatal, "Cannot present without using a swapchain!");
    }

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnPrePresent();
    }

    // Make sure the (n - FRAME_BUFFERING == n) present has finished before presenting anew.
    rhi.WaitForTokenOnCPU(presentTokens[currentFrameIndex]);

    if (!isSwapchainValid)
    {
        // Always submit deferred work even though we cant present
        SubmitDeferredWork();
        CleanupResources();
        return;
    }

    std::optional<RHITexture> backBuffer = swapChain->AcquireBackBuffer(currentFrameIndex);
    isSwapchainValid = backBuffer.has_value();

    // Before presenting we have to handle all the queued for submission command lists (and their dependencies).
    SubmitDeferredWork();

    if (backBuffer)
    {
        // Open a new command list that will be used to copy the presentTexture to the backbuffer, and presenting.
        RHITexture& presentTexture = GetRHITexture(GetCurrentPresentTexture().handle);

        // Copy the present texture to the backbuffer.
        // Must be a graphics queue in order to be able to move the backbuffer to the present state.
        NonNullPtr<RHICommandList> cmdList = commandPool->GetOrCreateCommandList(CommandQueueType::Graphics);
        cmdList->Open();

        // If the present texture has not been used yet, its data is in a invalid state.
        // Clear it with its clear color to ensure garbage is not shown.
        bool presentTextureHasBeenUsed = presentTexture.GetLastAccess() != RHIBarrierAccess::NoAccess;
        if (!presentTextureHasBeenUsed)
        {
            RHITextureBarrier barrier = presentTexture.GetClearTextureBarrier();
            cmdList->Barrier({}, { &barrier, 1 });
            cmdList->ClearTexture(RHITextureBinding(TextureBinding(GetCurrentPresentTexture()), presentTexture),
                                  TextureUsage::RenderTarget,
                                  presentTexture.GetDescription().clearValue);
        }

        std::array barriers = {
            RHITextureBarrier{
                presentTexture,
                RHIBarrierSync::Copy,
                RHIBarrierAccess::CopySource,
                RHITextureLayout::CopySource,
            },
            RHITextureBarrier{
                *backBuffer,
                RHIBarrierSync::Copy,
                RHIBarrierAccess::CopyDest,
                RHITextureLayout::CopyDest,
            },
        };
        cmdList->Barrier({}, barriers);
        cmdList->Copy(presentTexture, *backBuffer);
        cmdList->TextureBarrier(*backBuffer,
                                RHIBarrierSync::AllGraphics,
                                RHIBarrierAccess::NoAccess,
                                RHITextureLayout::Present);
        cmdList->Close();

        presentTokens[currentFrameIndex] = swapChain->Present(currentFrameIndex, rhi, cmdList, isFullscreenMode);
        commandPool->OnCommandListsSubmitted({ &cmdList, 1 }, { &presentTokens[currentFrameIndex], 1 });
    }

    currentFrameIndex = (currentFrameIndex + 1) % std::to_underlying(description.frameBuffering);

    CleanupResources();
}

CommandContext GfxBackend::BeginScopedCommandContext(CommandQueueType queueType,
                                                     SubmissionPolicy submissionPolicy,
                                                     std::span<SyncToken> dependencies)
{
    if (submissionPolicy == SubmissionPolicy::DeferToPresent && !description.useSwapChain)
    {
        VEX_LOG(Fatal,
                "Cannot use deferred submission policy when your graphics backend has no swapchain. Use "
                "SubmissionPolicy::Immediate instead!");
    }

    return CommandContext{ *this, commandPool->GetOrCreateCommandList(queueType), submissionPolicy, dependencies };
}

void GfxBackend::SubmitDeferredWork()
{
    std::vector dependencies(deferredSubmissionDependencies.begin(), deferredSubmissionDependencies.end());
    std::vector deferredSubmissionTokens = rhi.Submit(deferredSubmissionCommandLists, dependencies);
    commandPool->OnCommandListsSubmitted(deferredSubmissionCommandLists, deferredSubmissionTokens);

    for (auto& res : deferredSubmissionResources)
    {
        resourceCleanup.CleanupResource(std::move(res));
    }

    deferredSubmissionCommandLists.clear();
    deferredSubmissionDependencies.clear();
    deferredSubmissionResources.clear();
}

void GfxBackend::CleanupResources()
{
    // Flushes all resources that were queued up for deletion (using the max sync token that was used when the resource
    // was submitted for destruction).
    resourceCleanup.FlushResources(*descriptorPool, *allocator);
    commandPool->ReclaimCommandLists();

    // Send all shader errors to the user, we do this every time we cleanup, since cleanup occurs when we submit or
    // present.
    psCache->GetShaderCompiler().FlushCompilationErrors();
}

std::vector<SyncToken> GfxBackend::EndCommandContext(CommandContext& ctx)
{
    // We want to close a command list asap, to allow for driver optimizations.
    ctx.cmdList->Close();

    std::vector<SyncToken> syncTokens;

    // No swapchain means we submit asap, since no presents will occur.
    // If we have dependencies, we submit asap, since in order to insert dependency signals, we have to submit this
    // separately anyways.
    if (!description.useSwapChain || ctx.submissionPolicy == SubmissionPolicy::Immediate || !ctx.dependencies.empty())
    {
        syncTokens = rhi.Submit({ &ctx.cmdList, 1 }, ctx.dependencies);

        // Enqueue the command context's temporary resources for destruction.
        for (auto& res : ctx.temporaryResources)
        {
            resourceCleanup.CleanupResource(std::move(res));
        }

        commandPool->OnCommandListsSubmitted({ &ctx.cmdList, 1 }, syncTokens);

        // Users will not necessarily present (in the case we don't have a swapchain). So we instead cleanup our
        // resources at this point.
        CleanupResources();
    }
    else if (description.useSwapChain && (ctx.submissionPolicy == SubmissionPolicy::DeferToPresent))
    {
        // The submitting of a commandList when we have a swapchain should be batched as much as possible for further
        // driver optimizations (allowed to append them together during execution or reorder if no dependencies
        // exist).
        deferredSubmissionCommandLists.push_back(ctx.cmdList);
        deferredSubmissionDependencies.insert(ctx.dependencies.begin(), ctx.dependencies.end());
        deferredSubmissionResources.reserve(ctx.temporaryResources.size() + deferredSubmissionResources.size());
        for (auto& tempResource : ctx.temporaryResources)
        {
            deferredSubmissionResources.push_back(std::move(tempResource));
        }
    }
    else
    {
        VEX_LOG(Fatal, "Unsupported submission policy when submitting CommandContext...");
    }

    return syncTokens;
}

Texture GfxBackend::CreateTexture(TextureDescription description, ResourceLifetime lifetime)
{
    TextureUtil::ValidateTextureDescription(description);

    if (description.mips == 0)
    {
        description.mips =
            ComputeMipCount(std::make_tuple(description.width, description.height, description.GetDepth()));
    }

    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO(https://trello.com/c/K2jgp9ax): handle dynamic resources, includes specifying that the resource when
        // bound should use dynamic bindless indices and self-cleanup of the RHITexture should occur after the current
        // frame ends, would be used for transient resources inside our memory allocation strategy (avoids constant
        // reallocations).
        VEX_NOT_YET_IMPLEMENTED();
    }

    return Texture{ .handle = textureRegistry.AllocateElement(std::move(rhi.CreateTexture(*allocator, description))),
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

    return Buffer{ .handle = bufferRegistry.AllocateElement(std::move(rhi.CreateBuffer(*allocator, description))),
                   .description = std::move(description) };
}

ResourceMappedMemory GfxBackend::MapResource(const Buffer& buffer)
{
    RHIBuffer& rhiBuffer = GetRHIBuffer(buffer.handle);

    if (rhiBuffer.GetDescription().memoryLocality != ResourceMemoryLocality::CPUWrite &&
        rhiBuffer.GetDescription().memoryLocality != ResourceMemoryLocality::CPURead)
    {
        VEX_LOG(Fatal, "A non CPU-visible buffer cannot be mapped to.");
    }

    return { rhiBuffer };
}

ResourceMappedMemory GfxBackend::MapResource(const Texture& texture)
{
    RHITexture& rhiTexture = GetRHITexture(texture.handle);

    if (rhiTexture.GetDescription().memoryLocality != ResourceMemoryLocality::CPUWrite)
    {
        VEX_LOG(Fatal, "Texture needs to have CPUWrite locality to be mapped to directly");
    }

    return { rhiTexture };
}

void GfxBackend::DestroyTexture(const Texture& texture)
{
    resourceCleanup.CleanupResource(textureRegistry.ExtractElement(texture.handle));
}

void GfxBackend::DestroyBuffer(const Buffer& buffer)
{
    resourceCleanup.CleanupResource(bufferRegistry.ExtractElement(buffer.handle));
}

BindlessHandle GfxBackend::GetTextureBindlessHandle(const TextureBinding& bindlessResource)
{
    bindlessResource.Validate();

    auto& texture = GetRHITexture(bindlessResource.texture.handle);
    return texture.GetOrCreateBindlessView(bindlessResource, *descriptorPool);
}

BindlessHandle GfxBackend::GetBufferBindlessHandle(const BufferBinding& bindlessResource)
{
    bindlessResource.Validate();

    auto& buffer = GetRHIBuffer(bindlessResource.buffer.handle);
    return buffer.GetOrCreateBindlessView(bindlessResource.usage, bindlessResource.strideByteSize, *descriptorPool);
}

void GfxBackend::FlushGPU()
{
    VEX_LOG(Info, "Forcing a GPU flush...");

    SubmitDeferredWork();

    rhi.FlushGPU();

    CleanupResources();

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
        (isSwapchainValid &&
         (newWidth == description.platformWindow.width && newHeight == description.platformWindow.height)))
    {
        return;
    }

    // Destroy present textures
    for (auto& presentTex : presentTextures)
    {
        DestroyTexture(presentTex);
    }
    presentTextures.clear();

    FlushGPU();

    // Resize swapchain.
    swapChain->Resize(newWidth, newHeight);

    CreatePresentTextures();

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnResize(newWidth, newHeight);
    }

    description.platformWindow.width = newWidth;
    description.platformWindow.height = newHeight;
    isSwapchainValid = true;
}

Texture GfxBackend::GetCurrentPresentTexture()
{
    if (!description.useSwapChain)
    {
        VEX_LOG(Fatal, "Your backend was created without swapchain support. Backbuffers were not created.");
    }
    return presentTextures[currentFrameIndex];
}

bool GfxBackend::IsTokenComplete(const SyncToken& token) const
{
    return rhi.IsTokenComplete(token);
}

bool GfxBackend::AreTokensComplete(std::span<const SyncToken> tokens) const
{
    for (const auto& token : tokens)
    {
        if (!rhi.IsTokenComplete(token))
        {
            return false;
        }
    }
    return true;
}

void GfxBackend::WaitForTokenOnCPU(const SyncToken& syncToken)
{
    rhi.WaitForTokenOnCPU(syncToken);

    CleanupResources();
}

void GfxBackend::RecompileAllShaders()
{
    if (description.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache->GetShaderCompiler().MarkAllShadersDirty();
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
        psCache->GetShaderCompiler().SetCompilationErrorsCallback(callback);
    }
    else
    {
        VEX_LOG(Warning, "Cannot subscribe to shader errors when not in shader debug mode.");
    }
}

void GfxBackend::SetSamplers(std::span<const TextureSampler> newSamplers)
{
    psCache->GetResourceLayout().SetSamplers(newSamplers);
}

RenderExtension* GfxBackend::RegisterRenderExtension(UniqueHandle<RenderExtension>&& renderExtension)
{
    renderExtension->data = RenderExtensionData{ .rhi = &rhi, .descriptorPool = &*descriptorPool };
    renderExtension->Initialize();
    renderExtensions.push_back(std::move(renderExtension));
    return renderExtensions.back().get();
}

void GfxBackend::UnregisterRenderExtension(NonNullPtr<RenderExtension> renderExtension)
{
    // Included in <utility>, avoiding including heavy <algorithm>.
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
        psCache->GetShaderCompiler().MarkAllStaleShadersDirty();
    }
    else
    {
        VEX_LOG(Warning, "Cannot recompile changed shaders when not in shader debug mode.");
    }
}

PipelineStateCache& GfxBackend::GetPipelineStateCache()
{
    return *psCache;
}

RHITexture& GfxBackend::GetRHITexture(TextureHandle textureHandle)
{
    return textureRegistry[textureHandle];
}

RHIBuffer& GfxBackend::GetRHIBuffer(BufferHandle bufferHandle)
{
    return bufferRegistry[bufferHandle];
}

void GfxBackend::CreatePresentTextures()
{
    presentTextures.resize(std::to_underlying(description.frameBuffering));
    for (u8 presentTextureIndex = 0; presentTextureIndex < std::to_underlying(description.frameBuffering);
         ++presentTextureIndex)
    {
        TextureDescription presentTextureDesc = swapChain->GetBackBufferTextureDescription();
        presentTextureDesc.name = std::format("PresentTexture_{}", presentTextureIndex);
        presentTextureDesc.clearValue = description.presentTextureClearValue;
        presentTextures[presentTextureIndex] = CreateTexture(presentTextureDesc);
    }
}

} // namespace vex
