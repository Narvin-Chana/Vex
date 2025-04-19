#pragma once

#include <array>

#include <Vex/CommandQueueType.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

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

    // Flushes all current GPU commands.
    void FlushGPU();

    void SetVSync(bool useVSync);

    void OnWindowResized(u32 newWidth, u32 newHeight);

private:
    u32 width, height;

    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    u8 currentFrameIndex = 0;

    UniqueHandle<RHI> rhi;

    BackendDescription description;

    // =================================================
    //  RHI RESOURCES (should be destroyed before rhi)
    // =================================================

    // Synchronisation fence for each backbuffer frame (one per queue type).
    std::array<UniqueHandle<RHIFence>, CommandQueueTypes::Count> queueFrameFences;

    FrameResource<UniqueHandle<RHICommandPool>> commandPools;

    UniqueHandle<RHISwapChain> swapChain;
};

} // namespace vex