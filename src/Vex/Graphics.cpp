#include "Graphics.h"

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
#include <Vex/RHIImpl/RHITimestampQueryPool.h>
#include <Vex/RenderExtension.h>

namespace vex
{

Graphics::Graphics(const GraphicsCreateDesc& desc)
    : desc(desc)
    , rhi(desc.platformWindow.windowHandle, desc.enableGPUDebugLayer, desc.enableGPUBasedValidation)
    , resourceCleanup(rhi)
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
            desc.platformWindow.width,
            desc.platformWindow.height);

    commandPool.emplace(rhi.CreateCommandPool());

    descriptorPool = rhi.CreateDescriptorPool();

    psCache.emplace(&rhi, *descriptorPool, &resourceCleanup, desc.shaderCompilerSettings);

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
    if (!deferredSubmissionCommandLists.empty())
    {
        VEX_LOG(Warning,
                "Destroying Vex Graphics in the middle of a frame, this is valid, although not recommended."
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
    GEnableGPUScopedEvents = false;
}

void Graphics::Present(bool isFullscreenMode)
{
    if (!desc.useSwapChain)
    {
        VEX_LOG(Fatal, "Cannot present without using a swapchain!");
    }

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnPrePresent();
    }

    // Make sure the ((n - FRAME_BUFFERING) % FRAME_BUFFERING) present has finished before presenting anew.
    rhi.WaitForTokenOnCPU(presentTokens[currentFrameIndex]);

    // Before presenting we have to handle all the queued for submission command lists (and their dependencies).
    SubmitDeferredWork();

    if (std::optional<RHITexture> backBuffer = swapChain->AcquireBackBuffer(currentFrameIndex))
    {
        // Open a new command list that will be used to copy the presentTexture to the backbuffer, and presenting.
        RHITexture& presentTexture = GetRHITexture(GetCurrentPresentTexture().handle);

        // Copy the present texture to the backbuffer.
        // Must be a graphics queue in order to be able to move the backbuffer to the present state.
        NonNullPtr<RHICommandList> cmdList = commandPool->GetOrCreateCommandList(QueueType::Graphics);
        cmdList->Open();

        std::array barriers = {
            RHITextureBarrier{
                presentTexture,
                TextureSubresource{},
                RHIBarrierSync::Copy,
                RHIBarrierAccess::CopySource,
                RHITextureLayout::CopySource,
            },
            RHITextureBarrier{
                *backBuffer,
                TextureSubresource{},
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

    currentFrameIndex = (currentFrameIndex + 1) % std::to_underlying(desc.swapChainDesc.frameBuffering);

    // If our swapchain is stale, we must recreate it.
    if (swapChain->NeedsRecreation())
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

CommandContext Graphics::BeginScopedCommandContext(QueueType queueType,
                                                   SubmissionPolicy submissionPolicy,
                                                   std::span<SyncToken> dependencies)
{
    if (submissionPolicy == SubmissionPolicy::DeferToPresent && !desc.useSwapChain)
    {
        VEX_LOG(Fatal,
                "Cannot use deferred submission policy when your graphics backend has no swapchain. Use "
                "SubmissionPolicy::Immediate instead!");
    }

    return CommandContext{ *this,
                           commandPool->GetOrCreateCommandList(queueType),
                           *queryPool,
                           submissionPolicy,
                           dependencies };
}

void Graphics::SubmitDeferredWork()
{
    std::vector dependencies(deferredSubmissionDependencies.begin(), deferredSubmissionDependencies.end());
    std::vector deferredSubmissionTokens = rhi.Submit(deferredSubmissionCommandLists, dependencies);
    commandPool->OnCommandListsSubmitted(deferredSubmissionCommandLists, deferredSubmissionTokens);

    for (Buffer& tempBuffer : deferredSubmissionResources)
    {
        DestroyBuffer(tempBuffer);
    }

    deferredSubmissionCommandLists.clear();
    deferredSubmissionDependencies.clear();
    deferredSubmissionResources.clear();
}

void Graphics::CleanupResources()
{
    // Flushes all resources that were queued up for deletion (using the max sync token that was used when the resource
    // was submitted for destruction).
    resourceCleanup.FlushResources(*descriptorPool, *allocator);
    commandPool->ReclaimCommandLists();

    // Send all shader errors to the user, we do this every time we cleanup, since cleanup occurs when we submit or
    // present.
    psCache->GetShaderCompiler().FlushCompilationErrors();
}

std::vector<SyncToken> Graphics::EndCommandContext(CommandContext& ctx)
{
    // We want to close a command list asap, to allow for driver optimizations.
    ctx.cmdList->Close();

    std::vector<SyncToken> syncTokens;

    // No swapchain means we submit asap, since no presents will occur.
    // If we have dependencies, we submit asap, since in order to insert dependency signals, we have to submit this
    // separately anyways.
    if (!desc.useSwapChain || ctx.submissionPolicy == SubmissionPolicy::Immediate || !ctx.dependencies.empty())
    {
        syncTokens = rhi.Submit({ &ctx.cmdList, 1 }, ctx.dependencies);

        // Enqueue the command context's temporary resources for destruction.
        for (Buffer& tempBuffer : ctx.temporaryResources)
        {
            DestroyBuffer(tempBuffer);
        }

        commandPool->OnCommandListsSubmitted({ &ctx.cmdList, 1 }, syncTokens);

        // Users will not necessarily present (in the case we don't have a swapchain). So we instead cleanup our
        // resources at this point.
        CleanupResources();
    }
    else if (desc.useSwapChain && (ctx.submissionPolicy == SubmissionPolicy::DeferToPresent))
    {
        // The submission of a commandList when we have a swapchain should be batched as much as possible for further
        // driver optimizations (allowed to append them together during execution or reorder if no dependencies
        // exist).
        deferredSubmissionCommandLists.push_back(ctx.cmdList);
        deferredSubmissionDependencies.insert(ctx.dependencies.begin(), ctx.dependencies.end());
        deferredSubmissionResources.reserve(ctx.temporaryResources.size() + deferredSubmissionResources.size());
        for (Buffer& tempBuffer : ctx.temporaryResources)
        {
            deferredSubmissionResources.push_back(tempBuffer);
        }
    }
    else
    {
        VEX_LOG(Fatal, "Unsupported submission policy when submitting CommandContext...");
    }

    return syncTokens;
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

    return Texture{ .handle = textureRegistry.AllocateElement(std::move(rhi.CreateTexture(*allocator, desc))),
                    .desc = std::move(desc) };
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

    return Buffer{ .handle = bufferRegistry.AllocateElement(std::move(rhi.CreateBuffer(*allocator, desc))),
                   .desc = std::move(desc) };
}

ResourceMappedMemory Graphics::MapResource(const Buffer& buffer)
{
    RHIBuffer& rhiBuffer = GetRHIBuffer(buffer.handle);

    if (rhiBuffer.GetDesc().memoryLocality != ResourceMemoryLocality::CPUWrite &&
        rhiBuffer.GetDesc().memoryLocality != ResourceMemoryLocality::CPURead)
    {
        VEX_LOG(Fatal, "A non CPU-visible buffer cannot be mapped to.");
    }

    return { rhiBuffer };
}

ResourceMappedMemory Graphics::MapResource(const Texture& texture)
{
    RHITexture& rhiTexture = GetRHITexture(texture.handle);

    if (rhiTexture.GetDesc().memoryLocality != ResourceMemoryLocality::CPUWrite &&
        rhiTexture.GetDesc().memoryLocality != ResourceMemoryLocality::CPURead)
    {
        VEX_LOG(Fatal, "Texture needs to have CPUWrite or CPURead locality to be mapped to directly");
    }

    return { rhiTexture };
}

void Graphics::DestroyTexture(const Texture& texture)
{
    resourceCleanup.CleanupResource(textureRegistry.ExtractElement(texture.handle));
}

void Graphics::DestroyBuffer(const Buffer& buffer)
{
    resourceCleanup.CleanupResource(bufferRegistry.ExtractElement(buffer.handle));
}

BindlessHandle Graphics::GetTextureBindlessHandle(const TextureBinding& bindlessResource)
{
    BindingUtil::ValidateTextureBinding(bindlessResource, bindlessResource.texture.desc.usage);

    auto& texture = GetRHITexture(bindlessResource.texture.handle);
    return texture.GetOrCreateBindlessView(bindlessResource, *descriptorPool);
}

BindlessHandle Graphics::GetBufferBindlessHandle(const BufferBinding& bindlessResource)
{
    BindingUtil::ValidateBufferBinding(bindlessResource, bindlessResource.buffer.desc.usage);

    auto& buffer = GetRHIBuffer(bindlessResource.buffer.handle);
    return buffer.GetOrCreateBindlessView(bindlessResource, *descriptorPool);
}

void Graphics::FlushGPU()
{
    VEX_LOG(Info, "Forcing a GPU flush...");

    SubmitDeferredWork();

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

    for (auto& renderExtension : renderExtensions)
    {
        renderExtension->OnResize(newWidth, newHeight);
    }

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

bool Graphics::IsTokenComplete(const SyncToken& token) const
{
    return rhi.IsTokenComplete(token);
}

bool Graphics::AreTokensComplete(std::span<const SyncToken> tokens) const
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

void Graphics::SetSamplers(std::span<const TextureSampler> newSamplers)
{
    // TODO(https://trello.com/c/T1DY4QOT): This is not the cleanest, we need a linear sampler for the mip generation
    // shader, so we add it to the end of the users samplers. Instead we should probably have a way to declare a
    // specific sampler per-pass? Or support bindless samplers? Unsure...
    std::vector<TextureSampler> samplers = { newSamplers.begin(), newSamplers.end() };
    samplers.push_back(TextureSampler::CreateSampler(FilterMode::Linear, AddressMode::Clamp));
    builtInLinearSamplerSlot = samplers.size() - 1u;
    psCache->GetResourceLayout().SetSamplers(samplers);
}

RenderExtension* Graphics::RegisterRenderExtension(UniqueHandle<RenderExtension>&& renderExtension)
{
    renderExtension->data = RenderExtensionData{ .rhi = &rhi, .descriptorPool = &*descriptorPool };
    renderExtension->Initialize();
    renderExtensions.push_back(std::move(renderExtension));
    return renderExtensions.back().get();
}

void Graphics::UnregisterRenderExtension(NonNullPtr<RenderExtension> renderExtension)
{
    auto el = std::ranges::find_if(renderExtensions,
                                   [renderExtension](const UniqueHandle<RenderExtension>& ext)
                                   { return ext.get() == renderExtension; });
    if (el != renderExtensions.end())
    {
        renderExtensions.erase(el);
    }
}

std::expected<Query, QueryStatus> Graphics::GetTimestampValue(QueryHandle handle)
{
    VEX_CHECK(handle != vex::GInvalidQueryHandle, "Query handle must be valid when getting timestamp value");
    return queryPool->GetQueryData(handle);
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
    return textureRegistry[textureHandle];
}

RHIBuffer& Graphics::GetRHIBuffer(BufferHandle bufferHandle)
{
    return bufferRegistry[bufferHandle];
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

    auto ctx = BeginScopedCommandContext(vex::QueueType::Graphics, vex::SubmissionPolicy::Immediate);

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

        // Force the present textures to start zeroed out.
        ctx.ClearTexture({ .texture = presentTextures[presentTextureIndex] });
    }
}

} // namespace vex
