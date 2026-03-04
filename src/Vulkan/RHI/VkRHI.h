#pragma once

#include <utility>

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHI.h>
#include <RHI/RHIFwd.h>

#include <Vulkan/RHI/VkFence.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct PlatformWindowHandle;
} // namespace vex

namespace vex::vk
{

class VkRHI final : public RHIBase
{
public:
    VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    VkRHI(const VkRHI&) = delete;
    VkRHI& operator=(const VkRHI&) = delete;
    VkRHI(VkRHI&&) = default;
    VkRHI& operator=(VkRHI&&) = default;
    ~VkRHI();

    static std::vector<UniqueHandle<RHIPhysicalDevice>> EnumeratePhysicalDevices();
    virtual void Init() override;

    virtual RHISwapChain CreateSwapChain(SwapChainDesc& desc, const PlatformWindow& platformWindow) override;

    virtual RHICommandPool CreateCommandPool() override;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) override;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) override;
    virtual RHIRayTracingPipelineState CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key) override;
    virtual RHIResourceLayout CreateResourceLayout(RHIDescriptorPool& descriptorPool) override;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDesc& desc) override;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDesc& desc) override;

    virtual RHIDescriptorPool CreateDescriptorPool() override;

    virtual RHIAllocator CreateAllocator() override;

    virtual RHITimestampQueryPool CreateTimestampQueryPool(RHIAllocator& allocator) override;

    virtual RHIAccelerationStructure CreateAS(const ASDesc& desc) override;

    ::vk::Instance GetNativeInstance();
    ::vk::Device GetNativeDevice()
    {
        return *device;
    }
    const VkCommandQueue& GetCommandQueue(QueueType queueType)
    {
        return queues[std::to_underlying(queueType)];
    }
    ::vk::PhysicalDevice GetNativePhysicalDevice() const
    {
        return physDevice;
    }
    ::vk::PipelineCache GetNativePSOCache()
    {
        return *PSOCache;
    }

    virtual void WaitForTokenOnCPU(const SyncToken& syncToken) override;
    virtual bool IsTokenComplete(const SyncToken& syncToken) const override;
    virtual void WaitForTokenOnGPU(QueueType waitingQueue, const SyncToken& waitFor) override;
    virtual std::array<SyncToken, QueueTypes::Count> GetMostRecentSyncTokenPerQueue() const override;

    virtual std::vector<SyncToken> Submit(Span<const NonNullPtr<RHICommandList>> commandLists,
                                          Span<const SyncToken> dependencies) override;
    virtual void FlushGPU() override;

private:
    NonNullPtr<VkGPUContext> GetGPUContext();
    void InitWindow(const PlatformWindowHandle& windowHandle);

    void AddDependencyWait(std::vector<::vk::SemaphoreSubmitInfo>& waitSemaphores, SyncToken syncToken);
    SyncToken SubmitToQueue(QueueType queueType,
                            Span<const ::vk::CommandBufferSubmitInfo> commandBuffers,
                            Span<const ::vk::SemaphoreSubmitInfo> waitSemaphores,
                            std::vector<::vk::SemaphoreSubmitInfo> signalSemaphores = {});

    // TODO: Can't use maybe uninitialized since VkGPUContext has deleted move constructor (which in effect removes
    // VkRHI's defaulted move constructor). Would require using NonNullPtr in VkGPUContext, which causes changes in a
    // lot of places.
    UniqueHandle<VkGPUContext> ctx;

    struct DispatchRHILifetime
    {
        ~DispatchRHILifetime();
    } dispatch;

    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;
    ::vk::UniqueDevice device;
    ::vk::PhysicalDevice physDevice;
    ::vk::UniquePipelineCache PSOCache;

    std::array<VkCommandQueue, QueueTypes::Count> queues;
    std::optional<std::array<VkFence, QueueTypes::Count>> fences;

    // To be submitted when the next submission happens. Avoids submitting with an empty command buffer.
    std::array<std::vector<SyncToken>, QueueTypes::Count> pendingWaits;

    friend class VkSwapChain;
};

} // namespace vex::vk
