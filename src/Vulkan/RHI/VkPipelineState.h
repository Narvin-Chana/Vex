#pragma once

#include <RHI/RHIPipelineState.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkGraphicsPipelineState final : public RHIGraphicsPipelineStateBase
{
public:
    using Hasher = decltype([](const Key& key) -> std::size_t
    {
        // TODO(https://trello.com/c/2KeJ2cXy): Determine which keys are not needed in Vulkan. 
        // I'll let @alexandreLBarrett handle this since you implemented the Vk graphics code.
        std::size_t seed = 0;
        VEX_HASH_COMBINE(seed, key.vertexShader);
        VEX_HASH_COMBINE(seed, key.pixelShader);
        VEX_HASH_COMBINE(seed, key.vertexInputLayout);
        // Input assembly is bound dynamically.
        VEX_HASH_COMBINE(seed, key.rasterizerState);
        VEX_HASH_COMBINE(seed, key.depthStencilState);
        VEX_HASH_COMBINE(seed, key.colorBlendState);
        VEX_HASH_COMBINE(seed, key.renderTargetState);
        return seed;
    });

    VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    VkGraphicsPipelineState(VkGraphicsPipelineState&&) = default;
    VkGraphicsPipelineState& operator=(VkGraphicsPipelineState&&) = default;
    virtual void Compile(const Shader& vertexShader,
                         const Shader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline graphicsPipeline;

private:
    ::vk::Device device;
    ::vk::PipelineCache PSOCache;
};

class VkComputePipelineState final : public RHIComputePipelineStateInterface
{
public:
    VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    VkComputePipelineState(VkComputePipelineState&&) = default;
    VkComputePipelineState& operator=(VkComputePipelineState&&) = default;
    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline computePipeline;

private:
    ::vk::Device device;
    ::vk::PipelineCache PSOCache;
};

class VkRayTracingPipelineState final : public RHIRayTracingPipelineStateInterface
{
public:
    VkRayTracingPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    VkRayTracingPipelineState(VkRayTracingPipelineState&&) = default;
    VkRayTracingPipelineState& operator=(VkRayTracingPipelineState&&) = default;
    virtual void Compile(const RayTracingShaderCollection& shaderCollection,
                         RHIResourceLayout& resourceLayout,
                         ResourceCleanup& resourceCleanup) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;
};

} // namespace vex::vk