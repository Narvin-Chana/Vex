#pragma once

#include <array>
#include <vector>

#include <Vex/Buffer.h>
#include <Vex/CommandQueueType.h>
#include <Vex/Containers/FreeList.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/PipelineStateCache.h>
#include <Vex/PlatformWindow.h>
#include <Vex/Resource.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/Texture.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class CommandContext;
struct PhysicalDevice;
struct Texture;

struct BackendDescription
{
    PlatformWindow platformWindow;
    TextureFormat swapChainFormat;
    bool useVSync = false;
    FrameBuffering frameBuffering = FrameBuffering::Triple;
    bool enableGPUDebugLayer = !VEX_SHIPPING;
    bool enableGPUBasedValidation = !VEX_SHIPPING;
    // Enables shader hot reloading and includes debug symbols in shaders for graphics debugging purposes.
    bool enableShaderDebugging = !VEX_SHIPPING;
};

class GfxBackend
{
public:
    GfxBackend(UniqueHandle<RHI>&& newRHI, const BackendDescription& description);
    ~GfxBackend();

    void StartFrame();
    void EndFrame(bool isFullscreenMode);

    CommandContext BeginScopedCommandContext(CommandQueueType queueType);

    // Creates a new texture, the handle passed back should be kept.
    Texture CreateTexture(TextureDescription description, ResourceLifetime lifetime);
    // Destroys a texture, the handle passed in must be the one obtained from calling CreateTexture earlier.
    // Once destroyed the handle passed in is invalid and should no longer be used.
    void DestroyTexture(const Texture& texture);

    // Flushes all current GPU commands.
    void FlushGPU();

    void SetVSync(bool useVSync);

    void OnWindowResized(u32 newWidth, u32 newHeight);

    Texture GetCurrentBackBuffer();

    // Recompiles all shader which have changed since the last compilation. Useful for shader development and
    // hot-reloading. You generally want to avoid calling this too often if your application has many shaders.
    void RecompileChangedShaders();
    // Recompiles all shaders, could cause a big hitch depending on how many shaders your application uses.
    void RecompileAllShaders();

private:
    void EndCommandContext(RHICommandList& cmdList);

    PipelineStateCache& GetPipelineStateCache();
    RHICommandPool& GetCurrentCommandPool();

    RHITexture& GetRHITexture(TextureHandle textureHandle);
    RHIBuffer& GetRHIBuffer(BufferHandle bufferHandle);

    void CreateBackBuffers();

    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    u8 currentFrameIndex = 0;

    UniqueHandle<RHI> rhi;
    UniqueHandle<PhysicalDevice> physicalDevice;

    BackendDescription description;

    PipelineStateCache psCache;

    ResourceCleanup resourceCleanup;

    // =================================================
    //  RHI RESOURCES (should be destroyed before rhi)
    // =================================================

    // Synchronisation fence for each backbuffer frame (one per queue type).
    std::array<UniqueHandle<RHIFence>, CommandQueueTypes::Count> queueFrameFences;

    FrameResource<UniqueHandle<RHICommandPool>> commandPools;

    // Used for allocating/freeing bindless descriptors for resources.
    UniqueHandle<RHIDescriptorPool> descriptorPool;

    UniqueHandle<RHISwapChain> swapChain;
    std::vector<Texture> backBuffers;

    // Converts from the Handle to the actual underlying RHI resource.
    FreeList<UniqueHandle<RHITexture>, TextureHandle> textureRegistry;
    FreeList<UniqueHandle<RHIBuffer>, BufferHandle> bufferRegistry;

    inline static constexpr u32 DefaultRegistrySize = 1024;

    friend class CommandContext;
};

} // namespace vex