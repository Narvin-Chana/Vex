#pragma once

#include <unordered_map>

#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/Shaders/ShaderCompiler.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class Graphics;

class PipelineStateCache
{
public:
    PipelineStateCache(RHI* rhi, RHIDescriptorPool& descriptorPool, const ShaderCompilerSettings& compilerSettings);
    ~PipelineStateCache();

    PipelineStateCache(PipelineStateCache&&) = default;
    PipelineStateCache& operator=(PipelineStateCache&&) = default;

    RHIResourceLayout& GetResourceLayout();

    const RHIGraphicsPipelineState* GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key,
                                                             std::unique_ptr<RHIGraphicsPipelineState>& oldPSO);
    const RHIComputePipelineState* GetComputePipelineState(const RHIComputePipelineState::Key& key,
                                                           std::unique_ptr<RHIComputePipelineState>& oldPSO);
    const RHIRayTracingPipelineState* GetRayTracingPipelineState(
        const RHIRayTracingPipelineState::Key& key,
        RHIAllocator& allocator,
        std::unique_ptr<RHIRayTracingPipelineState>& oldPSO,
        std::vector<MaybeUninitialized<RHIBuffer>>& oldBuffers);

    ShaderCompiler& GetShaderCompiler();

private:
    // Converts a RayTracingPSOKey into the mirrored version of itself which contains all required shaders.
    std::optional<RayTracingShaderCollection> GetRayTracingShaderCollection(const RHIRayTracingPipelineState::Key& key);

    RHI* rhi;

    ShaderCompiler shaderCompiler;
    MaybeUninitialized<RHIResourceLayout> resourceLayout;
    std::unordered_map<RHIGraphicsPipelineState::Key, RHIGraphicsPipelineState, RHIGraphicsPipelineState::Hasher>
        graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, RHIComputePipelineState> computePSCache;
    std::unordered_map<RHIRayTracingPipelineState::Key, RHIRayTracingPipelineState> rayTracingPSCache;
};

} // namespace vex