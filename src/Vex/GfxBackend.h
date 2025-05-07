#pragma once

#include <array>

#include <Vex/Buffer.h>
#include <Vex/CommandQueueType.h>
#include <Vex/Containers/FreeList.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/PipelineStateCache.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/Resource.h>
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
    bool enableGPUDebugLayer = true;
    bool enableGPUBasedValidation = true;
};

class GfxBackend
{
public:
    GfxBackend(UniqueHandle<RHI>&& newRHI, const BackendDescription& description);
    ~GfxBackend();

    void StartFrame();
    void EndFrame();

    CommandContext BeginScopedCommandContext(CommandQueueType queueType);

    Texture CreateTexture(TextureDescription description, ResourceLifetime lifetime);

    // Flushes all current GPU commands.
    void FlushGPU();

    void SetVSync(bool useVSync);

    void OnWindowResized(u32 newWidth, u32 newHeight);

    Texture GetCurrentBackBuffer();

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