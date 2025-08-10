#pragma once

#include <RHI/RHIPipelineState.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkGraphicsPipelineState final : public RHIGraphicsPipelineStateInterface
{
    ::vk::PipelineCache PSOCache;
    ::vk::Device device;

public:
    VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    VkGraphicsPipelineState(VkGraphicsPipelineState&&) = default;
    VkGraphicsPipelineState& operator=(VkGraphicsPipelineState&&) = default;
    ~VkGraphicsPipelineState();
    virtual void Compile(const Shader& vertexShader,
                         const Shader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline graphicsPipeline;
};

class VkComputePipelineState final : public RHIComputePipelineStateInterface
{
    ::vk::PipelineCache PSOCache;
    ::vk::Device device;

public:
    VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    VkComputePipelineState(VkComputePipelineState&&) = default;
    VkComputePipelineState& operator=(VkComputePipelineState&&) = default;
    ~VkComputePipelineState();
    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline computePipeline;
};

} // namespace vex::vk