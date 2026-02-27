#include "Graphics.h"

#include <array>
#include <functional>
#include <mutex>
#include <utility>

#include <Vex/CommandContext.h>
#include <Vex/ResourceCleanup.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIAccelerationStructure.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Validation.h>
#include <Vex/Utility/Visitor.h>

#include <RHI/RHIAccelerationStructure.h>
#include <RHI/RHIBarrier.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHIPhysicalDevice.h>
#include <RHI/RHIScopedGPUEvent.h>

namespace vex
{

Graphics::Graphics(const GraphicsCreateDesc& desc)
    : desc(desc)
    , rhi(desc.platformWindow.windowHandle, desc.enableGPUDebugLayer, desc.enableGPUBasedValidation)
    , textureRegistry(DefaultRegistrySize)
    , bufferRegistry(DefaultRegistrySize)
{
    VEX_LOG(Info, "Creating Vex Graphics Backend with API Support:\n\tDX12: {} Vulkan: {}", VEX_DX12, VEX_VULKAN);

    std::string vexTargetName;
    if constexpr (VEX_DEBUG)
    {
        vexTargetName = "Debug (no optimizations with debug symbols)";
    }
    else if constexpr (VEX_DEVELOPMENT)
    {
        vexTargetName = "Development (full optimizations with debug symbols)";
    }
    else if constexpr (VEX_SHIPPING)
    {
        vexTargetName = "Shipping (full optimizations with no debug symbols)";
    }
    VEX_LOG(Info, "Running Vex in {}", vexTargetName);

    if (GPhysicalDevice)
    {
        VEX_LOG(Fatal, "Cannot launch multiple instances of Vex...");
    }

    std::vector<std::unique_ptr<RHIPhysicalDevice>> physicalDevices = RHI::EnumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        VEX_LOG(Fatal,
                "The underlying graphics API was unable to find atleast one physical device. Most likely due to "
                "not "
                "having Vex required features (see Vex documentation for required features)");
    }

    if (desc.specifiedDevice)
    {
        auto it = std::find_if(physicalDevices.begin(),
                               physicalDevices.end(),
                               [&](const std::unique_ptr<RHIPhysicalDevice>& device)
                               { return device->info == *desc.specifiedDevice; });
        if (it != physicalDevices.end())
        {
            GPhysicalDevice = std::move(*it);
        }
        else
        {
            VEX_LOG(Fatal,
                    "Provided device to graphics was not found internally. The device specified should be valid when "
                    "creating the Graphics.")
        }
    }

    if (!GPhysicalDevice)
    {
        // Obtain the best physical device.
        std::sort(physicalDevices.begin(), physicalDevices.end(), [](const auto& l, const auto& r) { return *l > *r; });

        GPhysicalDevice = std::move(physicalDevices[0]);
    }

#if !VEX_SHIPPING
    GPhysicalDevice->DumpPhysicalDeviceInfo();
#endif

    // Initializes RHI which includes creating logical device and swapchain (if applicable).
    rhi.Init();

    if (desc.useSwapChain)
    {
        VEX_LOG(Info,
                "Created graphics backend with width {} and height {}.",
                desc.platformWindow.width,
                desc.platformWindow.height);
    }
    else
    {
        VEX_LOG(Info, "Created headless graphics backend.");
    }

    commandPool.emplace(rhi.CreateCommandPool());

    descriptorPool = rhi.CreateDescriptorPool();

    psCache.emplace(&rhi, *descriptorPool, desc.shaderCompilerSettings);

    allocator = rhi.CreateAllocator();

    queryPool = rhi.CreateTimestampQueryPool(*allocator);

    // TODO(https://trello.com/c/T1DY4QOT): See the comment inside SetSampler().
    SetSamplers({});

    GEnableGPUScopedEvents = desc.enableGPUDebugLayer;

    if (desc.useSwapChain)
    {
        swapChain.emplace(rhi.CreateSwapChain(this->desc.swapChainDesc, desc.platformWindow));

        RecreatePresentTextures();
    }
}

Graphics::~Graphics()
{
    // Wait for work to be done before starting the deletion of resources.
    FlushGPU();

    // Clear the global physical device.
    GPhysicalDevice = nullptr;
    GEnableGPUScopedEvents = false;
}

void Graphics::Present(bool isFullscreenMode)
{
    if (!desc.useSwapChain)
    {
        VEX_LOG(Fatal, "Cannot present without using a swapchain!");
    }

    // Make sure the ((n - FRAME_BUFFERING) % FRAME_BUFFERING) present has finished before presenting anew.
    // TODO: Reasses the necessity of this
    // See: https://trello.com/c/E3ipWUc7
    rhi.WaitForTokenOnCPU(presentTokens[currentFrameIndex]);

    if (std::optional<RHITexture> backBuffer = swapChain->AcquireBackBuffer(currentFrameIndex))
    {
        // Open a new command list that will be used to copy the presentTexture to the backbuffer, and presenting.
        RHITexture& presentTexture = GetRHITexture(GetCurrentPresentTexture().handle);

        // Copy the present texture to the backbuffer.
        // Must be a graphics queue in order to be able to move the backbuffer to the present state.
        NonNullPtr<RHICommandList> cmdList = commandPool->GetOrCreateCommandList(QueueType::Graphics);
        cmdList->Open();

        RHIBarrierSync srcSync = RHIBarrierSync::None;
        RHIBarrierAccess srcAccess = RHIBarrierAccess::NoAccess;
        RHITextureLayout srcLayout = RHITextureLayout::Common;
        const auto [bbSrcSync, bbSrcAccess, bbSrcLayout] =
            backBufferState.Get(backBuffer->GetDesc(), TextureSubresource{});

        std::array barriers = {
            RHITextureBarrier{
                .texture = presentTexture,
                .subresource = TextureSubresource{},
                .srcSync = srcSync,
                .dstSync = RHIBarrierSync::Copy,
                .srcAccess = srcAccess,
                .dstAccess = RHIBarrierAccess::CopySource,
                .srcLayout = srcLayout,
                .dstLayout = RHITextureLayout::CopySource,
            },
            RHITextureBarrier{
                .texture = *backBuffer,
                .subresource = TextureSubresource{},
                .srcSync = bbSrcSync,
                .dstSync = RHIBarrierSync::Copy,
                .srcAccess = bbSrcAccess,
                .dstAccess = RHIBarrierAccess::CopyDest,
                .srcLayout = bbSrcLayout,
                .dstLayout = RHITextureLayout::CopyDest,
            },
        };
        cmdList->EmitBarriers({}, barriers, {});
        cmdList->Copy(presentTexture, *backBuffer);
        RHITextureBarrier backBufferBarrier{
            .texture = *backBuffer,
            .subresource = {},
            .srcSync = RHIBarrierSync::Copy,
            .dstSync = RHIBarrierSync::AllGraphics,
            .srcAccess = RHIBarrierAccess::CopyDest,
            .dstAccess = RHIBarrierAccess::NoAccess,
            .srcLayout = RHITextureLayout::CopyDest,
            .dstLayout = RHITextureLayout::Present,
        };
        RHITextureBarrier presentTextureBarrier{
            .texture = presentTexture,
            .subresource = {},
            .srcSync = RHIBarrierSync::Copy,
            .dstSync = RHIBarrierSync::None,
            .srcAccess = RHIBarrierAccess::CopySource,
            .dstAccess = RHIBarrierAccess::NoAccess,
            .srcLayout = RHITextureLayout::CopySource,
            .dstLayout = RHITextureLayout::Common,
        };
        cmdList->EmitBarriers({}, { backBufferBarrier, presentTextureBarrier }, {});
        cmdList->Close();

        presentTokens[currentFrameIndex] = swapChain->Present(currentFrameIndex, rhi, cmdList, isFullscreenMode);
        commandPool->OnCommandListsSubmitted({ &cmdList, 1 }, { &presentTokens[currentFrameIndex], 1 });

        // Certain swapchains reset the state of the backbuffer to Undefined after presenting.
        if (GPhysicalDevice->PresentResetsBackBufferToUndefined())
        {
            backBufferState.SetUniform(
                { RHIBarrierSync::None, RHIBarrierAccess::NoAccess, RHITextureLayout::Undefined });
        }
        else
        {
            backBufferState.SetUniform(
                { RHIBarrierSync::AllGraphics, RHIBarrierAccess::NoAccess, RHITextureLayout::Present });
        }
    }

    currentFrameIndex = (currentFrameIndex + 1) % std::to_underlying(desc.swapChainDesc.frameBuffering);

    // If our swapchain is stale, we must recreate it.
    if (swapChain->NeedsRecreation() && swapChain->CanRecreate())
    {
        VEX_LOG(Warning,
                "Swapchain is stale meaning it must be recreated. This can occur when changes occur with the "
                "output display, or changes to settings relating to HDR/Color Spaces.");
        FlushGPU();
        swapChain->RecreateSwapChain(desc.platformWindow.width, desc.platformWindow.height);
        // We can now update our present textures to match the swapchain's potentially new format.
        RecreatePresentTextures();
    }
    else
    {
        CleanupResources();
    }
}

CommandContext Graphics::CreateCommandContext(QueueType queueType)
{
    return CommandContext{ *this, commandPool->GetOrCreateCommandList(queueType), *queryPool };
}

std::optional<SyncToken> Graphics::FlushPendingInitializations()
{
    if (pendingInitializations.empty())
    {
        return std::nullopt;
    }

    // Transition all newly created textures to the default layout.
    CommandContext ctx = CreateCommandContext(QueueType::Graphics);
    for (auto& texture : pendingInitializations)
    {
        RHITextureBarrier barrier{
            .texture = GetRHITexture(texture.handle),
            .subresource = TextureSubresource{},
            .srcSync = RHIBarrierSync::None,
            .dstSync = RHIBarrierSync::None,
            .srcAccess = RHIBarrierAccess::NoAccess,
            .dstAccess = RHIBarrierAccess::NoAccess,
            .srcLayout = RHITextureLayout::Undefined,
            .dstLayout = RHITextureLayout::Common,
        };

        // A clear is needed for RT/DS-compatible textures before first use, as per DX12/Vulkan API requirements.
        if (texture.desc.usage & (TextureUsage::RenderTarget | TextureUsage::DepthStencil))
        {
            // Force the copy to consider the texture as initially in an undefined layout.
            // This only occurs the first time we initialize the texture, it will then be supposed that it is in the
            // default global state.
            ctx.textureStates[texture.handle].Set(texture.desc,
                                                  {},
                                                  RHITextureState{
                                                      .layout = RHITextureLayout::Undefined,
                                                  });

            // TODO: aspect handling is erroneous here.
            // See --gtest_filter=*ClearStencilImplicit*
            ctx.ClearTexture({ .texture = texture });

            // We still need to transition the texture to the default global state, just have to modify the src* values
            // since we just called ClearTexture.
            const auto& textureStatesMap = ctx.textureStates[texture.handle];
            const auto& states = textureStatesMap.Get(texture.desc, {});
            barrier.srcSync = states.sync;
            barrier.srcAccess = states.access;
            barrier.srcLayout = states.layout;
        }

        ctx.pendingTextureBarriers.push_back(std::move(barrier));
    }

    ctx.FlushBarriers();
    ctx.cmdList->Close();

    pendingInitializations.clear();

    auto token = rhi.Submit({ ctx.cmdList }, {})[0];
    commandPool->OnCommandListsSubmitted({ ctx.cmdList }, { token });

    return token;
}

void Graphics::PrepareCommandContextForSubmission(CommandContext& ctx)
{
    VEX_ASSERT(ctx.cmdList->IsOpen(), "Error on submit: attempting to submit an already closed command context...");

    // Reset all touched textures to the universal default texture layout.
    for (auto& touchedTex : ctx.touchedTextures)
    {
        ctx.EnqueueTextureBarrier(touchedTex,
                                  TextureSubresource{},
                                  RHIBarrierSync::None,
                                  RHIBarrierAccess::NoAccess,
                                  RHITextureLayout::Common);
    }

    // Flush all resource writes using a global barrier.
    ctx.EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::AllCommands,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = RHIBarrierAccess::MemoryRead,
    });

    // Flush barriers before submitting.
    ctx.FlushBarriers();

    // We want to close a command list asap, to allow for driver optimizations.
    ctx.cmdList->Close();
}

void Graphics::CleanupResources()
{
    // Flush all potential CPU work that was enqueued to the GPU timeline, this can include RHI resource cleanup.
    ExecuteCPUWork();
    commandPool->ReclaimCommandLists();
    // Send all shader errors to the user, we do this every time we cleanup, since cleanup occurs when we submit or
    // present.
    psCache->GetShaderCompiler().FlushCompilationErrors();
}

Texture Graphics::CreateTexture(TextureDesc desc, ResourceLifetime lifetime)
{
    TextureUtil::ValidateTextureDescription(desc);

    if (desc.mips == 0)
    {
        desc.mips = ComputeMipCount(std::make_tuple(desc.width, desc.height, desc.GetDepth()));
    }

    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO(https://trello.com/c/K2jgp9ax): handle dynamic resources, includes specifying that the resource when
        // bound should use dynamic bindless indices and self-cleanup of the RHITexture should occur after the current
        // frame ends, would be used for transient resources inside our memory allocation strategy (avoids constant
        // reallocations).
        VEX_NOT_YET_IMPLEMENTED();
    }

    Texture texture{
        .handle = textureRegistry.AllocateElement(std::make_unique<RHITexture>(rhi.CreateTexture(*allocator, desc))),
        .desc = std::move(desc),
    };
    pendingInitializations.push_back(texture);
    return texture;
}

Buffer Graphics::CreateBuffer(BufferDesc desc, ResourceLifetime lifetime)
{
    BufferUtil::ValidateBufferDesc(desc);

    if (lifetime == ResourceLifetime::Dynamic)
    {
        // TODO(https://trello.com/c/K2jgp9ax): handle dynamic resources, includes specifying that the resource when
        // bound should use dynamic bindless indices and self-cleanup of the RHITexture should occur after the current
        // frame ends, would be used for transient resources inside our memory allocation strategy (avoids constant
        // reallocations).
        VEX_NOT_YET_IMPLEMENTED();
    }

    return Buffer{ .handle =
                       bufferRegistry.AllocateElement(std::make_unique<RHIBuffer>(rhi.CreateBuffer(*allocator, desc))),
                   .desc = std::move(desc) };
}

AccelerationStructure Graphics::CreateAccelerationStructure(const ASDesc& desc)
{
    VEX_CHECK(GPhysicalDevice->IsFeatureSupported(Feature::RayTracing),
              "Your GPU does not support ray tracing, unable to create an acceleration structure!");
    return {
        .handle = accelerationStructureRegistry.AllocateElement(
            std::make_unique<RHIAccelerationStructure>(rhi.CreateAS(desc))),
        .desc = desc,
    };
}

MappedMemory Graphics::MapResource(const Buffer& buffer)
{
    RHIBuffer& rhiBuffer = GetRHIBuffer(buffer.handle);

    if (rhiBuffer.GetDesc().memoryLocality != ResourceMemoryLocality::CPUWrite &&
        rhiBuffer.GetDesc().memoryLocality != ResourceMemoryLocality::CPURead)
    {
        VEX_LOG(Fatal, "A non CPU-visible buffer cannot be mapped to.");
    }

    return { rhiBuffer };
}

void Graphics::DestroyTexture(const Texture& texture)
{
    if (!texture.handle.IsValid())
    {
        return;
    }
    ScheduleCPUCallback([&, rhiTexture = *textureRegistry.ExtractElement(texture.handle)]() mutable
                        { CleanupResource(std::move(rhiTexture), *descriptorPool, *allocator); },
                        rhi.GetMostRecentSyncTokenPerQueue());
}

void Graphics::DestroyBuffer(const Buffer& buffer)
{
    if (!buffer.handle.IsValid())
    {
        return;
    }
    ScheduleCPUCallback([&, rhiBuffer = *bufferRegistry.ExtractElement(buffer.handle)]() mutable
                        { CleanupResource(std::move(rhiBuffer), *descriptorPool, *allocator); },
                        rhi.GetMostRecentSyncTokenPerQueue());
}

void Graphics::DestroyAccelerationStructure(const AccelerationStructure& accelerationStructure)
{
    if (!accelerationStructure.handle.IsValid())
    {
        return;
    }
    ScheduleCPUCallback(
        [&, rhiAS = *accelerationStructureRegistry.ExtractElement(accelerationStructure.handle)]() mutable
        { CleanupResource(std::move(rhiAS), *descriptorPool, *allocator); },
        rhi.GetMostRecentSyncTokenPerQueue());
}

BindlessHandle Graphics::GetBindlessHandle(const TextureBinding& bindlessResource)
{
    BindingUtil::ValidateTextureBinding(bindlessResource, bindlessResource.texture.desc.usage);

    auto& texture = GetRHITexture(bindlessResource.texture.handle);
    return texture.GetOrCreateBindlessView(bindlessResource, *descriptorPool);
}

BindlessHandle Graphics::GetBindlessHandle(const BufferBinding& bindlessResource)
{
    BindingUtil::ValidateBufferBinding(bindlessResource, bindlessResource.buffer.desc.usage);

    auto& buffer = GetRHIBuffer(bindlessResource.buffer.handle);
    return buffer.GetOrCreateBindlessView(bindlessResource, *descriptorPool);
}

BindlessHandle Graphics::GetBindlessHandle(const AccelerationStructure& accelerationStructure)
{
    return GetRHIAccelerationStructure(accelerationStructure.handle)
        .GetRHIBuffer()
        .GetOrCreateBindlessView({}, *descriptorPool);
}

std::vector<BindlessHandle> Graphics::GetBindlessHandles(Span<const ResourceBinding> bindlessResources)
{
    std::vector<BindlessHandle> handles;
    handles.reserve(bindlessResources.size());
    for (const auto& binding : bindlessResources)
    {
        std::visit(Visitor{ [&handles, this](const BufferBinding& bufferBinding)
                            { handles.emplace_back(GetBindlessHandle(bufferBinding)); },
                            [&handles, this](const TextureBinding& texBinding)
                            { handles.emplace_back(GetBindlessHandle(texBinding)); },
                            [&handles, this](const ASBinding& asBinding)
                            { handles.emplace_back(GetBindlessHandle(asBinding.accelerationStructure)); } },
                   binding.binding);
    }
    return handles;
}

SyncToken Graphics::Submit(CommandContext& ctx, Span<const SyncToken> dependencies)
{
    auto tokens = Submit(std::span(&ctx, 1), dependencies);
    VEX_ASSERT(tokens.size() == 1);
    return tokens[0];
}

std::vector<SyncToken> Graphics::Submit(Span<CommandContext> commandContexts, Span<const SyncToken> dependencies)
{
    // Process any pending textures.
    std::optional<SyncToken> pendingInitializationToken = FlushPendingInitializations();

    // Collect and prepare all command contexts for submission.
    std::vector<NonNullPtr<RHICommandList>> cmdLists;
    for (auto& ctx : commandContexts)
    {
        PrepareCommandContextForSubmission(ctx);
        cmdLists.push_back(ctx.cmdList);
    }

    std::vector<SyncToken> tokens;
    if (pendingInitializationToken.has_value())
    {
        std::vector<SyncToken> finalDependencies{ dependencies.begin(), dependencies.end() };
        finalDependencies.push_back(pendingInitializationToken.value());
        tokens = rhi.Submit(cmdLists, finalDependencies);
    }
    else
    {
        tokens = rhi.Submit(cmdLists, dependencies);
    }

    // Collect the temporary resources of all command contexts in this phase.
    std::vector<CleanupVariant> phaseTemporaryResources;
    for (auto& ctx : commandContexts)
    {
        for (auto& buffer : ctx.temporaryBuffers)
        {
            phaseTemporaryResources.push_back(*bufferRegistry.ExtractElement(buffer.handle));
        }
        for (auto& resource : ctx.temporaryResources)
        {
            phaseTemporaryResources.emplace_back(std::move(resource));
        }
    }

    // Send them to be cleaned-up once the GPU has done executing.
    ScheduleCPUCallback(
        [this, resources = std::move(phaseTemporaryResources)]() mutable
        {
            for (auto& resource : resources)
            {
                CleanupResource(std::move(resource), *descriptorPool, *allocator);
            }
        },
        tokens);

    commandPool->OnCommandListsSubmitted(cmdLists, tokens);

    CleanupResources();

    return tokens;
}

void Graphics::ScheduleCPUCallback(CPUCallback&& callback, Span<const SyncToken> tokens)
{
    pendingCPUWork.emplace_back(std::move(callback), std::vector<SyncToken>{ tokens.begin(), tokens.end() });
}

void Graphics::EnqueueCPUWork(CPUCallback&& callback, Span<const SyncToken> tokens)
{
    ScheduleCPUCallback(std::move(callback), tokens);
}

void Graphics::ExecuteCPUWork()
{
    std::erase_if(pendingCPUWork,
                  [this](PendingCPUWork& work)
                  {
                      if (AreTokensComplete(work.tokens))
                      {
                          work.callback();
                          return true;
                      }
                      return false;
                  });
}

void Graphics::FlushCPUWork()
{
    while (!pendingCPUWork.empty())
    {
        ExecuteCPUWork();

        if (!pendingCPUWork.empty())
        {
            // Provide hint to the OS to avoid spinning when the GPU has not yet finished all work.
            std::this_thread::yield();
        }
    }
}

void Graphics::FlushGPU()
{
    VEX_LOG(Info, "Forcing a GPU flush...");

    rhi.FlushGPU();

    CleanupResources();
}

void Graphics::SetUseVSync(bool useVSync)
{
    desc.swapChainDesc.useVSync = useVSync;
}

bool Graphics::GetUseVSync() const
{
    return desc.swapChainDesc.useVSync;
}

void Graphics::SetUseHDRIfSupported(bool newValue)
{
    desc.swapChainDesc.useHDRIfSupported = newValue;
}

bool Graphics::GetUseHDRIfSupported() const
{
    return desc.swapChainDesc.useHDRIfSupported;
}

void Graphics::SetPreferredHDRColorSpace(vex::ColorSpace newValue)
{
    desc.swapChainDesc.preferredColorSpace = newValue;
}

ColorSpace Graphics::GetPreferredHDRColorSpace() const
{
    return desc.swapChainDesc.preferredColorSpace;
}

ColorSpace Graphics::GetCurrentHDRColorSpace() const
{
    return swapChain->GetCurrentColorSpace();
}

void Graphics::OnWindowResized(u32 newWidth, u32 newHeight)
{
    // Do not resize if any of the dimensions is 0, or if the resize gives us the same window size as we have
    // currently.
    if (newWidth == 0 || newHeight == 0 ||
        (newWidth == desc.platformWindow.width && newHeight == desc.platformWindow.height))
    {
        return;
    }

    FlushGPU();

    // Take advantage of the resize to cleanup no longer needed resources.
    CleanupResources();

    // Recreate our swapchain.
    swapChain->RecreateSwapChain(newWidth, newHeight);
    RecreatePresentTextures();

    desc.platformWindow.width = newWidth;
    desc.platformWindow.height = newHeight;
}

Texture Graphics::GetCurrentPresentTexture()
{
    if (!desc.useSwapChain)
    {
        VEX_LOG(Fatal, "Your backend was created without swapchain support. Backbuffers were not created.");
    }
    return presentTextures[currentFrameIndex];
}

bool Graphics::IsRayTracingSupported() const
{
    return GPhysicalDevice->IsFeatureSupported(Feature::RayTracing);
}

bool Graphics::IsTokenComplete(const SyncToken& token) const
{
    return rhi.IsTokenComplete(token);
}

bool Graphics::AreTokensComplete(Span<const SyncToken> tokens) const
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

void Graphics::WaitForTokenOnCPU(const SyncToken& syncToken)
{
    rhi.WaitForTokenOnCPU(syncToken);

    CleanupResources();
}

void Graphics::RecompileAllShaders()
{
    if (desc.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache->GetShaderCompiler().MarkAllShadersDirty();
    }
    else
    {
        VEX_LOG(Warning, "Cannot recompile shaders when not in shader debug mode.");
    }
}

void Graphics::SetShaderCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback)
{
    if (desc.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache->GetShaderCompiler().SetCompilationErrorsCallback(callback);
    }
    else
    {
        VEX_LOG(Warning, "Cannot subscribe to shader errors when not in shader debug mode.");
    }
}

void Graphics::SetSamplers(Span<const TextureSampler> newSamplers)
{
    // TODO(https://trello.com/c/T1DY4QOT): This is not the cleanest, we need a linear sampler for the mip
    // generation shader, so we add it to the end of the users samplers. Instead we should probably have a way to
    // declare a specific sampler per-pass? Or support bindless samplers? Unsure...
    std::vector<TextureSampler> samplers = { newSamplers.begin(), newSamplers.end() };
    samplers.push_back(TextureSampler::CreateSampler(FilterMode::Linear, AddressMode::Clamp));
    builtInLinearSamplerSlot = samplers.size() - 1u;
    psCache->GetResourceLayout().SetSamplers(samplers);
}

std::expected<Query, QueryStatus> Graphics::GetTimestampValue(QueryHandle handle)
{
    VEX_CHECK(handle != vex::GInvalidQueryHandle, "Query handle must be valid when getting timestamp value");
    return queryPool->GetQueryData(handle);
}

std::vector<PhysicalDeviceInfo> Graphics::GetSupportedDevices()
{
    std::vector<std::unique_ptr<RHIPhysicalDevice>> devices = RHI::EnumeratePhysicalDevices();

    std::vector<PhysicalDeviceInfo> infos(devices.size());
    std::transform(devices.begin(),
                   devices.end(),
                   infos.begin(),
                   [](const std::unique_ptr<RHIPhysicalDevice>& device) { return device->info; });
    return infos;
}

void Graphics::RecompileChangedShaders()
{
    if (desc.shaderCompilerSettings.enableShaderDebugging)
    {
        psCache->GetShaderCompiler().MarkAllStaleShadersDirty();
    }
    else
    {
        VEX_LOG(Warning, "Cannot recompile changed shaders when not in shader debug mode.");
    }
}

PipelineStateCache& Graphics::GetPipelineStateCache()
{
    return *psCache;
}

RHITexture& Graphics::GetRHITexture(TextureHandle textureHandle)
{
    return *textureRegistry[textureHandle];
}

RHIBuffer& Graphics::GetRHIBuffer(BufferHandle bufferHandle)
{
    return *bufferRegistry[bufferHandle];
}

RHIAccelerationStructure& Graphics::GetRHIAccelerationStructure(ASHandle asHandle)
{
    return *accelerationStructureRegistry[asHandle];
}

void Graphics::RecreatePresentTextures()
{
    if (!presentTextures.empty())
    {
        // Check if its even necessary to recreate the present textures.
        const auto& presentDesc = presentTextures[0].desc;
        const auto& swapChainDesc = swapChain->GetBackBufferTextureDescription();
        if (presentDesc.format == swapChainDesc.format && presentDesc.width == swapChainDesc.width &&
            presentDesc.height == swapChainDesc.height)
        {
            return;
        }
    }

    // Clear current present textures.
    for (const Texture& tex : presentTextures)
    {
        DestroyTexture(tex);
    }

    // Create new present textures.
    presentTextures.resize(std::to_underlying(desc.swapChainDesc.frameBuffering));
    presentTokens.clear();
    presentTokens.resize(std::to_underlying(desc.swapChainDesc.frameBuffering));
    for (u8 presentTextureIndex = 0; presentTextureIndex < std::to_underlying(desc.swapChainDesc.frameBuffering);
         ++presentTextureIndex)
    {
        TextureDesc presentTextureDesc = swapChain->GetBackBufferTextureDescription();
        presentTextureDesc.name = std::format("PresentTexture_{}", presentTextureIndex);
        presentTextureDesc.clearValue = desc.presentTextureClearValue;
        // Allow for present texture to be used for all usage types.
        presentTextureDesc.usage =
            TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite | TextureUsage::RenderTarget;
        presentTextures[presentTextureIndex] = CreateTexture(presentTextureDesc);
        // Present texture will be initialized when pendingInitializations are flushed on next submit.
    }
}

} // namespace vex
