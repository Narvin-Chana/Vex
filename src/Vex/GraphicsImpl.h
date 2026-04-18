#pragma once

namespace vex
{

static constexpr u32 GDefaultRegistrySize = 1024;

struct GraphicsImpl
{
    explicit GraphicsImpl(const GraphicsCreateDesc& desc);
    ~GraphicsImpl();

    void Present();
    [[nodiscard]] Texture CreateTexture(const TextureDesc& textureDesc,
                                        ResourceLifetime lifetime = ResourceLifetime::Static);
    void DestroyTexture(const Texture& texture);
    [[nodiscard]] Buffer CreateBuffer(const BufferDesc& bufferDesc,
                                      ResourceLifetime lifetime = ResourceLifetime::Static);
    void DestroyBuffer(const Buffer& buffer);
    [[nodiscard]] AccelerationStructure CreateAccelerationStructure(const AccelerationStructureDesc& asDesc);
    void DestroyAccelerationStructure(const AccelerationStructure& accelerationStructure);
    [[nodiscard]] MappedMemory MapResource(const Buffer& buffer);
    [[nodiscard]] BindlessHandle GetBindlessHandle(const TextureBinding& bindlessResource);
    [[nodiscard]] BindlessHandle GetBindlessHandle(const BufferBinding& bindlessResource);
    [[nodiscard]] BindlessHandle GetBindlessHandle(const AccelerationStructure& accelerationStructure);
    [[nodiscard]] std::vector<BindlessHandle> GetBindlessHandles(Span<const ResourceBinding> bindlessResources);
    [[nodiscard]] BindlessHandle GetBindlessSampler(const BindlessTextureSampler& sampler);
    void SetStaticSamplers(Span<const StaticTextureSampler> staticSamplers);
    SyncToken Submit(CommandContext& ctx, Span<const SyncToken> dependencies = {});
    std::vector<SyncToken> Submit(Span<CommandContext> commandContexts, Span<const SyncToken> dependencies = {});
    [[nodiscard]] bool IsTokenComplete(const SyncToken& token) const;
    [[nodiscard]] bool AreTokensComplete(Span<const SyncToken> tokens) const;
    void WaitForTokenOnCPU(const SyncToken& syncToken);
    void FlushGPU();
    void SetUseVSync(bool useVSync);
    [[nodiscard]] bool GetUseVSync() const;
    void SetUseHDRIfSupported(bool newValue);
    [[nodiscard]] bool GetUseHDRIfSupported() const;
    void SetPreferredHDRColorSpace(ColorSpace newValue);
    [[nodiscard]] ColorSpace GetPreferredHDRColorSpace() const;
    [[nodiscard]] ColorSpace GetCurrentHDRColorSpace() const;
    void OnWindowResized(u32 newWidth, u32 newHeight);
    [[nodiscard]] bool UsesSwapChain() const;
    [[nodiscard]] Texture GetCurrentPresentTexture();
    [[nodiscard]] bool IsRayTracingSupported() const;
    [[nodiscard]] std::expected<Query, QueryStatus> GetTimestampValue(QueryHandle handle);

    void EnqueueCPUWork(CPUCallback&& callback, Span<const SyncToken> tokens);
    void ExecuteCPUWork();

    std::optional<SyncToken> FlushPendingInitializations();
    void PrepareCommandContextForSubmission(CommandContext& ctx);
    void Cleanup();

    PipelineStateCache& GetPipelineStateCache();

    RHITexture& GetRHITexture(TextureHandle textureHandle);
    RHIBuffer& GetRHIBuffer(BufferHandle bufferHandle);
    RHIAccelerationStructure& GetRHIAccelerationStructure(AccelerationStructureHandle asHandle);
    void RecreatePresentTextures();

    static std::vector<PhysicalDeviceInfo> GetSupportedDevices();

    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    // Only valid if the backend uses a swapchain and not able to be used for anything OTHER than consecutive
    // presents/backbuffers.
    u8 currentFrameIndex = 0;

    GraphicsCreateDesc desc;

    RHI rhi;

    // =========================================================================
    //  RHI RESOURCES (should be destroyed before rhi and their order matters)
    // =========================================================================

    MaybeUninitialized<RHICommandPool> commandPool;

    // Used for allocating/freeing bindless descriptors for resources and samplers.
    MaybeUninitialized<RHIDescriptorPool> descriptorPool;

    MaybeUninitialized<PipelineStateCache> psCache;

    MaybeUninitialized<RHIAllocator> allocator;

    MaybeUninitialized<RHISwapChain> swapChain;

    MaybeUninitialized<RHITimestampQueryPool> queryPool;

    // Converts from the Handle to the actual underlying RHI resource.
    FreeList<std::unique_ptr<RHITexture>, TextureHandle> textureRegistry;
    FreeList<std::unique_ptr<RHIBuffer>, BufferHandle> bufferRegistry;
    FreeList<std::unique_ptr<RHIAccelerationStructure>, AccelerationStructureHandle> accelerationStructureRegistry;

    std::vector<Texture> pendingInitializations;

    std::vector<Texture> presentTextures;
    std::vector<SyncToken> presentTokens;

    TextureStateMap backBufferState;

    struct PendingCPUWork
    {
        CPUCallback callback;
        std::vector<SyncToken> tokens;
    };
    std::vector<PendingCPUWork> pendingCPUWork;

    std::unordered_map<BindlessTextureSampler, BindlessHandle> bindlessSamplers;

    friend class CommandContext;
    friend struct ResourceBindingUtils;
    friend class TextureReadbackContext;
    friend class BufferReadbackContext;
};


}