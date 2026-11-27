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
                                                       std::optional<IndexFormat> indexFormat,
                                                       std::vector<RHIDeferredRelease>& deferredReleases);
    RHIGraphicsPipelineState* GetGraphicsPipelineState(const DispatchMeshDesc& drawDesc,
                                                       const RenderTargetState& renderTargetState,
                                                       std::vector<RHIDeferredRelease>& deferredReleases);
    RHIComputePipelineState* GetComputePipelineState(const ShaderView& computeShader,
                                                     std::vector<RHIDeferredRelease>& deferredReleases);
    RHIRayTracingPipelineState* GetRayTracingPipelineState(const RayTracingShaderCollection& shaderCollection,
                                                           RHIAllocator& allocator,
                                                           std::vector<RHIDeferredRelease>& deferredReleases,
                                                           RHIRayTracingPipelineState::ShaderTableBuffers& oldBuffers);

    MaybeUninitialized<RHIResourceLayout> resourceLayout;

private:
    RHI* rhi;

    std::unordered_map<RHIGraphicsPipelineState::Key, RHIGraphicsPipelineState, RHIGraphicsPipelineState::Hasher>
        graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, RHIComputePipelineState> computePSCache;
    std::unordered_map<RayTracingPSOKey, RHIRayTracingPipelineState> rayTracingPSCache;
};

} // namespace vex