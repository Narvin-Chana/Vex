#pragma once

#include <Vex/RHI/RHIPipelineState.h>

namespace vex::vk
{

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
public:
    VkComputePipelineState(const Key& key);
    virtual void Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout) override;
};

} // namespace vex::vk