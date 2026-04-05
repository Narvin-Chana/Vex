#pragma once

#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIPipelineState.h>

#include <Vulkan/RHI/VkShaderTable.h>
#include <Vulkan/VkGPUContext.h>
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

    VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache psoCache);
    VkGraphicsPipelineState(VkGraphicsPipelineState&&) = default;
    VkGraphicsPipelineState& operator=(VkGraphicsPipelineState&&) = default;
    virtual void Compile(const Shader& vertexShader,
                         const Shader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual std::unique_ptr<RHIGraphicsPipelineState> Cleanup() override;

    ::vk::UniquePipeline graphicsPipeline;

private:
    ::vk::Device device;
    ::vk::PipelineCache psoCache;
};

class VkComputePipelineState final : public RHIComputePipelineStateBase
{
public:
    VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache psoCache);
    VkComputePipelineState(VkComputePipelineState&&) = default;
    VkComputePipelineState& operator=(VkComputePipelineState&&) = default;
    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual std::unique_ptr<RHIComputePipelineState> Cleanup() override;

    ::vk::UniquePipeline computePipeline;

private:
    ::vk::Device device;
    ::vk::PipelineCache psoCache;
};

class VkRayTracingPipelineState final : public RHIRayTracingPipelineStateBase
{
public:
    VkRayTracingPipelineState(const Key& key, NonNullPtr<VkGPUContext> ctx, ::vk::PipelineCache psoCache);
    VkRayTracingPipelineState(VkRayTracingPipelineState&&) = default;
    VkRayTracingPipelineState& operator=(VkRayTracingPipelineState&&) = default;
    virtual std::vector<MaybeUninitialized<RHIBuffer>> Compile(const RayTracingShaderCollection& shaderCollection,
                                                               RHIResourceLayout& resourceLayout,
                                                               RHIAllocator& allocator) override;
    virtual std::unique_ptr<RHIRayTracingPipelineState> Cleanup() override;

    ::vk::UniquePipeline rtPipeline;

    MaybeUninitialized<VkShaderTable> rayGenTable;
    MaybeUninitialized<VkShaderTable> rayMissTable;
    MaybeUninitialized<VkShaderTable> groupHitTable;
    MaybeUninitialized<VkShaderTable> rayCallableTable;

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::PipelineCache psoCache;
};

} // namespace vex::vk