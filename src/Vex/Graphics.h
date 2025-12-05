#pragma once

#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

#include <Vex/Containers/FreeList.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Containers/Span.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/PipelineStateCache.h>
#include <Vex/Platform/PlatformWindow.h>
#include <Vex/QueueType.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandPool.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RHIImpl/RHITimestampQueryPool.h>
#include <Vex/Synchronization.h>
#include <Vex/Utility/MaybeUninitialized.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{
class TextureReadbackContext;

class CommandContext;
struct PhysicalDevice;
struct Texture;
struct TextureSampler;
struct TextureBinding;
struct BufferBinding;
struct ResourceBinding;
class RenderExtension;

struct GraphicsCreateDesc
{
    PlatformWindow platformWindow;
    // Enables or disables using a swapchain. If this is disabled, all calls to "Present" are invalid.
    bool useSwapChain = true;
    SwapChainDesc swapChainDesc;

    // Clear value to use for present textures.
    TextureClearValue presentTextureClearValue = { .flags = TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };

    // Enables the GPU debug layer.
    bool enableGPUDebugLayer = !VEX_SHIPPING;
    // Enables GPU-based validation. Can be very costly in terms of performance.
    bool enableGPUBasedValidation = VEX_DEBUG;

    ShaderCompilerSettings shaderCompilerSettings;
};

class Graphics
{
public:
    Graphics(const GraphicsCreateDesc& desc);
    ~Graphics();

    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    Graphics(Graphics&&) = default;
    Graphics& operator=(Graphics&&) = default;

    // Presents the current presentTexture to the swapchain. Will stall if the GPU's next backbuffer is not yet ready
    // (depends on your FrameBuffering). If you use an HDR swapchain, this will apply HDR conversions, if necessary,
    // before copying the present texture to the swapChain.
    void Present(bool isFullscreenMode);

    // Create a CommandContext in which GPU commands can be recorded. The command context must later on be submitted to
    // the GPU by calling vex::Graphics::Submit().
    [[nodiscard]] CommandContext CreateCommandContext(QueueType queueType);

    // Creates a new texture with the specified description.
    [[nodiscard]] Texture CreateTexture(TextureDesc desc, ResourceLifetime lifetime = ResourceLifetime::Static);

    // Creates a new buffer with the specified description.
    [[nodiscard]] Buffer CreateBuffer(BufferDesc desc, ResourceLifetime lifetime = ResourceLifetime::Static);

    // Writes data to buffer memory. This only supports buffers with ResourceMemoryLocality::CPUWrite.
    [[nodiscard]] ResourceMappedMemory MapResource(const Buffer& buffer);
    [[nodiscard]] ResourceMappedMemory MapResource(const Texture& texture);

    // Destroys a texture, the handle passed in must be the one obtained from calling CreateTexture earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyTexture(const Texture& texture);

    // Destroys a buffer, the handle passed in must be the one obtained from calling CreateBuffer earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyBuffer(const Buffer& buffer);

    // Allows users to fetch the bindless handles for a texture binding. This bindless handle remains valid as long as
    // the resource itself is alive.
    [[nodiscard]] BindlessHandle GetBindlessHandle(const TextureBinding& bindlessResource);

    // Allows users to fetch the bindless handles for a buffer binding. This bindless handle remains valid as long as
    // the resource itself is alive.
    [[nodiscard]] BindlessHandle GetBindlessHandle(const BufferBinding& bindlessResource);

    // Allows users to fetch the bindless handles for multiple resource bindings. These bindless handles remain valid as
    // long as the resources themselves are alive.
    [[nodiscard]] std::vector<BindlessHandle> GetBindlessHandles(Span<const ResourceBinding> bindlessResources);

    // Allows you to submit the command context to the GPU, receiving a SyncToken which can be optionally used to track
    // work completion.
    SyncToken Submit(CommandContext& ctx, std::span<SyncToken> dependencies = {});

    // Allows you to submit the command contexts to the GPU, receiving a SyncToken which can be optionally used to track
    // work completion.
    // TODO(https://trello.com/c/gKJUXMD0): This is a bit annoying, std::spans in C++23 are not creatable from an
    // initializer list, forcing us to have a helper function. I believe this was added in C++26. In the meantime, we
    // should use a custom span that can be created from an initializer list.
    std::vector<SyncToken> Submit(std::span<const NonNullPtr<CommandContext>> ctxSpan,
                                  std::span<SyncToken> dependencies = {});

    // Allows you to submit the command contexts to the GPU, receiving a SyncToken which can be optionally used to track
    // work completion.
    std::vector<SyncToken> Submit(std::initializer_list<const NonNullPtr<CommandContext>> ctxs,
                                  std::span<SyncToken> dependencies = {});

    // Has the passed-in sync token been executed on the GPU yet?
    [[nodiscard]] bool IsTokenComplete(const SyncToken& token) const;

    // Have the passed-in sync tokens been executed on the GPU yet?
    [[nodiscard]] bool AreTokensComplete(Span<const SyncToken> tokens) const;

    // Waits for the passed in token to be done.
    void WaitForTokenOnCPU(const SyncToken& syncToken);

    // Flushes all currently submitted GPU commands.
    void FlushGPU();

    // Enables or disables vertical sync when presenting. Could lead to having to recreate the swapchain after the next
    // present.
    void SetUseVSync(bool useVSync);
    [[nodiscard]] bool GetUseVSync() const;

    // Determines if the swapchain is allowed to use an HDR format. Could lead to having to recreate the swapchain after
    // the next present.
    void SetUseHDRIfSupported(bool newValue);
    [[nodiscard]] bool GetUseHDRIfSupported() const;

    // Changes the preferred color space, although if unavailable Vex will fallback to other color spaces. Could lead to
    // having to recreate the swapchain after the next present.
    void SetPreferredHDRColorSpace(ColorSpace newValue);
    [[nodiscard]] ColorSpace GetPreferredHDRColorSpace() const;

    // Returns the currently used HDR color-space.
    [[nodiscard]] ColorSpace GetCurrentHDRColorSpace() const;

    // Called when the underlying window resizes, allows the swapchain to be resized.
    void OnWindowResized(u32 newWidth, u32 newHeight);
    [[nodiscard]] bool UsesSwapChain() const
    {
        return desc.useSwapChain;
    };

    // Obtains the current present texture handle. If the swapchain is enabled, Vex uses a present texture which is
    // copied to the backbuffer when presenting.
    [[nodiscard]] Texture GetCurrentPresentTexture();

    // Recompiles all shader which have changed since the last compilation. Useful for shader development and
    // hot-reloading. You generally want to avoid calling this too often if your application has many shaders.
    void RecompileChangedShaders();
    // Recompiles all shaders, could cause a big hitch depending on how many shaders your application uses.
    void RecompileAllShaders();
    void SetShaderCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback);

    void SetSamplers(Span<const TextureSampler> newSamplers);

    // Register a custom RenderExtension, it will be automatically unregistered when the graphics backend is destroyed.
    RenderExtension* RegisterRenderExtension(UniqueHandle<RenderExtension>&& renderExtension);
    // You can manually unregister a RenderExtension by passing in the pointer returned on creation.
    void UnregisterRenderExtension(NonNullPtr<RenderExtension> renderExtension);

    // Returns Query or status if query is not yet ready
    [[nodiscard]] std::expected<Query, QueryStatus> GetTimestampValue(QueryHandle handle);

private:
    void PrepareCommandContextForSubmission(CommandContext& ctx);
    void CleanupResources();

    PipelineStateCache& GetPipelineStateCache();

    RHITexture& GetRHITexture(TextureHandle textureHandle);
    RHIBuffer& GetRHIBuffer(BufferHandle bufferHandle);

    void RecreatePresentTextures();

    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    // Only valid if the backend uses a swapchain and not able to be used for anything OTHER than consecutive
    // presents/backbuffers.
    u8 currentFrameIndex = 0;

    GraphicsCreateDesc desc;

    RHI rhi;

    ResourceCleanup resourceCleanup;

    // =================================================
    //  RHI RESOURCES (should be destroyed before rhi and order matters)
    // =================================================

    MaybeUninitialized<RHICommandPool> commandPool;

    // Used for allocating/freeing bindless descriptors for resources.
    MaybeUninitialized<RHIDescriptorPool> descriptorPool;

    MaybeUninitialized<PipelineStateCache> psCache;

    MaybeUninitialized<RHIAllocator> allocator;

    MaybeUninitialized<RHISwapChain> swapChain;

    MaybeUninitialized<RHITimestampQueryPool> queryPool;

    // Converts from the Handle to the actual underlying RHI resource.
    FreeList<RHITexture, TextureHandle> textureRegistry;
    FreeList<RHIBuffer, BufferHandle> bufferRegistry;

    std::vector<Texture> presentTextures;
    std::vector<SyncToken> presentTokens;

    std::vector<UniqueHandle<RenderExtension>> renderExtensions;

    u32 builtInLinearSamplerSlot = ~0;

    static constexpr u32 DefaultRegistrySize = 1024;

    friend class CommandContext;
    friend struct ResourceBindingUtils;
    friend class TextureReadbackContext;
    friend class BufferReadbackContext;
};

} // namespace vex