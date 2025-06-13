#pragma once

#include <Vex/RHI/RHIPipelineState.h>

#include "VkHeaders.h"

namespace vex::vk
{

class ResourceCleanup;

class VkGraphicsPipelineState : public RHIGraphicsPipelineState
{
public:
    VkGraphicsPipelineState(const Key& key);
    virtual void Compile(const RHIShader& vertexShader,
                         const RHIShader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
};

class VkComputePipelineState : public RHIComputePipelineState
{
    ::vk::PipelineCache PSOCache;
    ::vk::Device device;

public:
    VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache);
    virtual void Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ::vk::UniquePipeline computePipeline;
};

} // namespace vex::vk