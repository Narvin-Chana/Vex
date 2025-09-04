#pragma once

#include <span>
#include <vector>

#include <Vex/CommandQueueType.h>
#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct BufferDescription;
struct TextureDescription;
struct PhysicalDevice;
struct GraphicsPipelineStateKey;
struct ComputePipelineStateKey;
struct RayTracingPassDescription;
using RayTracingPipelineStateKey = RayTracingPassDescription;
struct SwapChainDescription;
struct PlatformWindow;
enum class ShaderCompilerBackend : u8;
struct ShaderEnvironment;
struct SyncToken;

struct RHIBase
{
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) = 0;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() = 0;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) = 0;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) = 0;
    virtual RHIRayTracingPipelineState CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key) = 0;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) = 0;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDescription& description) = 0;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDescription& description) = 0;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() = 0;

    virtual RHIAllocator CreateAllocator() = 0;

    virtual void ModifyShaderCompilerEnvironment(ShaderCompilerBackend compilerBackend,
                                                 ShaderEnvironment& shaderEnv) = 0;

    virtual void WaitForTokenOnCPU(const SyncToken& syncToken) = 0;
    virtual bool IsTokenComplete(const SyncToken& syncToken) = 0;
    virtual void WaitForTokenOnGPU(CommandQueueType waitingQueue, const SyncToken& waitFor) = 0;

    virtual std::array<SyncToken, CommandQueueTypes::Count> GetMostRecentSyncTokenPerQueue() const = 0;

    virtual std::vector<SyncToken> Submit(std::span<NonNullPtr<RHICommandList>> commandLists,
                                          std::span<SyncToken> dependencies) = 0;

    virtual void FlushGPU() = 0;
};

} // namespace vex