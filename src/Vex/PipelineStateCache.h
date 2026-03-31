#pragma once

#include <unordered_map>

#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class Graphics;

class PipelineStateCache
{
public:
    PipelineStateCache(NonNullPtr<RHI> rhi, RHIDescriptorPool& descriptorPool);
    ~PipelineStateCache();

    PipelineStateCache(PipelineStateCache&&) = default;
    PipelineStateCache& operator=(PipelineStateCache&&) = default;

    RHIGraphicsPipelineState* GetGraphicsPipelineState(const DrawDesc& drawDesc,
                                                       const RenderTargetState& renderTargetState,
                                                       std::unique_ptr<RHIGraphicsPipelineState>& oldPSO);
    RHIComputePipelineState* GetComputePipelineState(const ShaderView& computeShader,
                                                     std::unique_ptr<RHIComputePipelineState>& oldPSO);
    RHIRayTracingPipelineState* GetRayTracingPipelineState(const RayTracingShaderCollection& shaderCollection,
                                                           RHIAllocator& allocator,
                                                           std::unique_ptr<RHIRayTracingPipelineState>& oldPSO,
                                                           std::vector<MaybeUninitialized<RHIBuffer>>& oldBuffers);

    MaybeUninitialized<RHIResourceLayout> resourceLayout;

private:
    RHI* rhi;

    std::unordered_map<RHIGraphicsPipelineState::Key, RHIGraphicsPipelineState, RHIGraphicsPipelineState::Hasher>
        graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, RHIComputePipelineState> computePSCache;
    std::unordered_map<RayTracingPSOKey, RHIRayTracingPipelineState> rayTracingPSCache;
};

} // namespace vex