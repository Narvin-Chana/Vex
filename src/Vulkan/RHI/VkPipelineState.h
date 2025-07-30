#pragma once

#include <RHI/RHIPipelineState.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

class VkGraphicsPipelineState final : public RHIGraphicsPipelineStateInterface
{
public:
    VkGraphicsPipelineState(const Key& key);
    ~VkGraphicsPipelineState();
    virtual void Compile(const Shader& vertexShader,
                         const Shader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;
};

class VkComputePipelineState final : public RHIComputePipelineStateInterface
{
    ::vk::PipelineCache PSOCache;
    ::vk::Device device;

public:
    VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    ~VkComputePipelineState();
    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline computePipeline;
};

} // namespace vex::vk