#pragma once

#include <array>

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHI/RHI.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class RHIFence;

struct BackendDescription
{
    PlatformWindow platformWindow;
    TextureFormat swapChainFormat;
    FrameBuffering frameBuffering = FrameBuffering::Triple;
    bool enableGPUDebugLayer = true;
    bool enableGPUBasedValidation = true;
};

class GfxBackend
{
public:
    GfxBackend(UniqueHandle<RenderHardwareInterface>&& newRHI, const BackendDescription& description);
    ~GfxBackend();

    void StartFrame();
    void EndFrame();

    // Flushes all current GPU commands.
    void FlushGPU();

private:
    // Index of the current frame, possible values depends on buffering:
    //  {0} if single buffering
    //  {0, 1} if double buffering
    //  {0, 1, 2} if triple buffering
    u8 currentFrameIndex = 0;

    UniqueHandle<RenderHardwareInterface> rhi;

    BackendDescription description;

    // =================================================
    //  RHI RESOURCES (should be destroyed before rhi)
    // =================================================

    // Synchronisation fence for each backbuffer frame (one per queue type).
    std::array<UniqueHandle<RHIFence>, CommandQueueTypes::Count> queueFrameFences;

    FrameResource<UniqueHandle<RHICommandPool>> commandPools;
};

} // namespace vex