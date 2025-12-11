#pragma once

#include <Vex/Containers/Span.h>
#include <vector>

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/QueueType.h>
#include <Vex/Types.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct BufferDesc;
struct TextureDesc;
struct PhysicalDevice;
struct GraphicsPipelineStateKey;
struct ComputePipelineStateKey;
struct RayTracingPassDescription;
using RayTracingPipelineStateKey = RayTracingPassDescription;
struct SwapChainDesc;
struct PlatformWindow;
enum class ShaderCompilerBackend : u8;
struct ShaderEnvironment;
struct SyncToken;
struct BLASDesc;
struct TLASDesc;

struct RHIBase
{
    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() = 0;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) = 0;

    virtual RHISwapChain CreateSwapChain(SwapChainDesc& desc, const PlatformWindow& platformWindow) = 0;

    virtual RHICommandPool CreateCommandPool() = 0;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) = 0;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) = 0;
    virtual RHIRayTracingPipelineState CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key) = 0;
    virtual RHIResourceLayout CreateResourceLayout(RHIDescriptorPool& descriptorPool) = 0;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDesc& desc) = 0;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDesc& desc) = 0;

    virtual RHIDescriptorPool CreateDescriptorPool() = 0;

    virtual RHIAllocator CreateAllocator() = 0;

    virtual RHITimestampQueryPool CreateTimestampQueryPool(RHIAllocator& allocator) = 0;

    virtual RHIAccelerationStructure CreateBLAS(const BLASDesc& desc) = 0;
    virtual RHIAccelerationStructure CreateTLAS(const TLASDesc& desc) = 0;

    virtual void WaitForTokenOnCPU(const SyncToken& syncToken) = 0;
    virtual bool IsTokenComplete(const SyncToken& syncToken) const = 0;
    virtual void WaitForTokenOnGPU(QueueType waitingQueue, const SyncToken& waitFor) = 0;

    virtual std::array<SyncToken, QueueTypes::Count> GetMostRecentSyncTokenPerQueue() const = 0;

    virtual std::vector<SyncToken> Submit(Span<const NonNullPtr<RHICommandList>> commandLists,
                                          Span<const SyncToken> dependencies) = 0;

    virtual void FlushGPU() = 0;
};

} // namespace vex