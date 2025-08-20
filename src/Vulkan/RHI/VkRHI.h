#pragma once

#include <Vex/NonNullPtr.h>
#include <Vex/RHIFwd.h>

#include <RHI/RHI.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct BufferDescription;
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

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) override;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() override;

    virtual RHIGraphicsPipelineState CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key) override;
    virtual RHIComputePipelineState CreateComputePipelineState(const ComputePipelineStateKey& key) override;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(RHIDescriptorPool& descriptorPool) override;

    virtual RHITexture CreateTexture(RHIAllocator& allocator, const TextureDescription& description) override;
    virtual RHIBuffer CreateBuffer(RHIAllocator& allocator, const BufferDescription& description) override;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() override;

    virtual RHIAllocator CreateAllocator() override;

    virtual void ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args,
                                                 std::vector<ShaderDefine>& defines) override;

    ::vk::Instance GetNativeInstance()
    {
        return *instance;
    }
    ::vk::Device GetNativeDevice()
    {
        return *device;
    }
    const VkCommandQueue& GetCommandQueue(CommandQueueType queueType)
    {
        return commandQueues[std::to_underlying(queueType)];
    }
    ::vk::PhysicalDevice GetNativePhysicalDevice()
    {
        return physDevice;
    }
    ::vk::PipelineCache GetNativePSOCache()
    {
        return *PSOCache;
    }

    virtual u32 AcquireNextFrame(RHISwapChain& swapChain, u32 currentFrameIndex) override;
    virtual void SubmitAndPresent(std::span<RHICommandList*> commandLists,
                                  RHISwapChain& swapChain,
                                  u32 currentFrameIndex,
                                  bool isFullscreenMode) override;
    virtual void FlushGPU() override;

private:
    NonNullPtr<VkGPUContext> GetGPUContext();
    void InitWindow(const PlatformWindowHandle& windowHandle);

    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;
    ::vk::UniqueDevice device;
    ::vk::PhysicalDevice physDevice;
    ::vk::UniquePipelineCache PSOCache;

    std::array<VkCommandQueue, CommandQueueTypes::Count> commandQueues;
};

} // namespace vex::vk
