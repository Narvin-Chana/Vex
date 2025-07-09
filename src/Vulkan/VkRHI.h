#pragma once

#include <Vex/RHI/RHI.h>

#include "VkCommandQueue.h"
#include "VkGPUContext.h"
#include "VkHeaders.h"

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::vk
{

class VkRHI : public RHI
{
public:
    VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~VkRHI() override;

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

    virtual UniqueHandle<RHISwapChain> CreateSwapChain(const SwapChainDescription& description,
                                                       const PlatformWindow& platformWindow) override;

    virtual UniqueHandle<RHICommandPool> CreateCommandPool() override;

    virtual UniqueHandle<RHIShader> CreateShader(const ShaderKey& key) override;
    virtual UniqueHandle<RHIGraphicsPipelineState> CreateGraphicsPipelineState(
        const GraphicsPipelineStateKey& key) override;
    virtual UniqueHandle<RHIComputePipelineState> CreateComputePipelineState(
        const ComputePipelineStateKey& key) override;
    virtual UniqueHandle<RHIResourceLayout> CreateResourceLayout(const FeatureChecker& featureChecker,
                                                                 RHIDescriptorPool& descriptorPool) override;

    virtual UniqueHandle<RHITexture> CreateTexture(const TextureDescription& description) override;

    virtual UniqueHandle<RHIDescriptorPool> CreateDescriptorPool() override;

    virtual void ExecuteCommandList(RHICommandList& commandList) override;

    virtual UniqueHandle<RHIFence> CreateFence(u32 numFenceIndices) override;
    virtual void SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;
    virtual void WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex) override;
    virtual void AddAdditionnalShaderCompilerArguments(std::vector<LPCWSTR>& args) override;

private:
    VkGPUContext& GetGPUContext();
    void InitWindow(const PlatformWindowHandle& windowHandle);

    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;
    ::vk::UniqueDevice device;
    ::vk::PhysicalDevice physDevice;
    ::vk::UniquePipelineCache PSOCache;

    std::array<VkCommandQueue, CommandQueueTypes::Count> commandQueues;
};

} // namespace vex::vk
