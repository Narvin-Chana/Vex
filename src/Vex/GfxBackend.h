#pragma once

#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/Containers/FreeList.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/NonNullPtr.h>
#include <Vex/PipelineStateCache.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHIFwd.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/SubmissionPolicy.h>
#include <Vex/Synchronization.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class CommandContext;
struct PhysicalDevice;
struct Texture;
struct TextureSampler;
struct BufferBinding;
class RenderExtension;

struct BackendDescription
{
    PlatformWindow platformWindow;
    bool useSwapChain = true;
    TextureFormat swapChainFormat;
    bool useVSync = false;
    FrameBuffering frameBuffering = FrameBuffering::Triple;
    bool enableGPUDebugLayer = !VEX_SHIPPING;
    bool enableGPUBasedValidation = !VEX_SHIPPING;
    ShaderCompilerSettings shaderCompilerSettings;
};

class GfxBackend
{
public:
    GfxBackend(const BackendDescription& description);
    ~GfxBackend();

    // Presents the current presentTexture to the swapchain. Will stall if the GPU's next backbuffer is not yet ready
    // (depends on your FrameBuffering).
    void Present(bool isFullscreenMode);

    // Begin a scoped CommandContext in which GPU commands can be submitted. The command context will automatically
    // submit its commands upon destruction if you use immediate submission policy. The Deferred submission policy will
    // instead submit all command queues batched together at swapchain present time.
    CommandContext BeginScopedCommandContext(CommandQueueType queueType,
                                             SubmissionPolicy submissionPolicy = SubmissionPolicy::DeferToPresent,
                                             std::span<SyncToken> dependencies = {});

    // Creates a new texture, the handle passed back should be kept.
    Texture CreateTexture(TextureDescription description, ResourceLifetime lifetime);

    // Creates a new buffer with specified description, the buffer will be refered using the Buffer object returned
    Buffer CreateBuffer(BufferDescription description, ResourceLifetime lifetime);

    // Writes data to buffer memory. This only supports buffers with CPUWrite ResourceLocality
    ResourceMappedMemory MapResource(const Buffer& buffer);
    ResourceMappedMemory MapResource(const Texture& texture);

    // Destroys a texture, the handle passed in must be the one obtained from calling CreateTexture earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyTexture(const Texture& texture);

    // Destroys a buffer, the handle passed in must be the one obtained from calling CreateBuffer earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyBuffer(const Buffer& buffer);

    // Allows users to fetch the bindless handles for a texture binding
    BindlessHandle GetTextureBindlessHandle(const TextureBinding& bindlessResource);
    // Allows users to fetch the bindless handles for a buffer binding
    BindlessHandle GetBufferBindlessHandle(const BufferBinding& bindlessResource);

    // Waits for the passed in token to be done.
    void WaitForTokenOnCPU(const SyncToken& syncToken);
    // Flushes all currently submitted GPU commands.
    void FlushGPU();

    // Enables or disables vsync when presenting.
    void SetVSync(bool useVSync);

    // Called when the underlying window resizes, allows the swapchain to be resized.
    void OnWindowResized(u32 newWidth, u32 newHeight);

    // Obtains the current present texture handle. If the swapchain is enabled, Vex uses a present texture which is
    // copied to the backbuffer when presenting.
    Texture GetCurrentPresentTexture();

    // Recompiles all shader which have changed since the last compilation. Useful for shader development and
    // hot-reloading. You generally want to avoid calling this too often if your application has many shaders.
    void RecompileChangedShaders();
    // Recompiles all shaders, could cause a big hitch depending on how many shaders your application uses.
    void RecompileAllShaders();
    void SetShaderCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback);

    void SetSamplers(std::span<TextureSampler> newSamplers);

    // Register a custom RenderExtension, it will be automatically unregistered when the graphics backend is destroyed.
    RenderExtension* RegisterRenderExtension(UniqueHandle<RenderExtension>&& renderExtension);
    // You can manually unregister a RenderExtension by passing in the pointer returned on creation.
    void UnregisterRenderExtension(NonNullPtr<RenderExtension> renderExtension);

private:
    void DitchDeferredWork();
    void SubmitDeferredWork();
    void CleanupResources();

    std::vector<SyncToken> EndCommandContext(CommandContext& ctx);

    PipelineStateCache& GetPipelineStateCache();

    RHITexture& GetRHITexture(TextureHandle textureHandle);
    RHIBuffer& GetRHIBuffer(BufferHandle bufferHandle);

    void CreatePresentTextures();

    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    // Only valid if the backend uses a swapchain and not able to be used for anything OTHER than consecutive
    // presents/backbuffers.
    u8 currentFrameIndex = 0;

    RHI rhi;

    BackendDescription description;

    PipelineStateCache psCache;

    ResourceCleanup resourceCleanup;

    // =================================================
    //  RHI RESOURCES (should be destroyed before rhi)
    // =================================================

    UniqueHandle<RHICommandPool> commandPool;

    // Used for allocating/freeing bindless descriptors for resources.
    UniqueHandle<RHIDescriptorPool> descriptorPool;

    MaybeUninitialized<RHIAllocator> allocator;

    UniqueHandle<RHISwapChain> swapChain;

    // Converts from the Handle to the actual underlying RHI resource.
    FreeList<RHITexture, TextureHandle> textureRegistry;
    FreeList<RHIBuffer, BufferHandle> bufferRegistry;

    std::vector<Texture> presentTextures;
    std::vector<SyncToken> presentTokens;

    // We submit our command lists in batch at the end of frame, to reduce driver overhead.
    std::vector<NonNullPtr<RHICommandList>> deferredSubmissionCommandLists;
    std::unordered_set<SyncToken> deferredSubmissionDependencies;
    std::vector<ResourceCleanup::CleanupVariant> deferredSubmissionResources;

    std::vector<UniqueHandle<RenderExtension>> renderExtensions;

    static constexpr u32 DefaultRegistrySize = 1024;

    friend class CommandContext;
    friend struct ResourceBindingUtils;
    bool isSwapchainValid = true;
};

} // namespace vex