#pragma once

#include <unordered_map>

#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/Shaders/ShaderCompiler.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class PipelineStateCache
{
public:
    PipelineStateCache(RHI* rhi,
                       RHIDescriptorPool& descriptorPool,
                       ResourceCleanup* resourceCleanup,
                       const ShaderCompilerSettings& compilerSettings);
    ~PipelineStateCache();

    PipelineStateCache(PipelineStateCache&&) = default;
    PipelineStateCache& operator=(PipelineStateCache&&) = default;

    RHIResourceLayout& GetResourceLayout();

    const RHIGraphicsPipelineState* GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key);
    const RHIComputePipelineState* GetComputePipelineState(const RHIComputePipelineState::Key& key);
    const RHIRayTracingPipelineState* GetRayTracingPipelineState(const RHIRayTracingPipelineState::Key& key,
                                                                 RHIAllocator& allocator);

    ShaderCompiler& GetShaderCompiler();

private:
    // Converts a RayTracingPSOKey into the mirrored version of itself which contains all required shaders.
    std::optional<RayTracingShaderCollection> GetRayTracingShaderCollection(const RHIRayTracingPipelineState::Key& key);

    // Can't use NonNullPtr, as this field can potentially be empty during the time it takes for the RHI to be
    // initialized.
    RHI* rhi;

    ResourceCleanup* resourceCleanup;

    ShaderCompiler shaderCompiler;
    MaybeUninitialized<RHIResourceLayout> resourceLayout;
    std::unordered_map<RHIGraphicsPipelineState::Key, RHIGraphicsPipelineState, RHIGraphicsPipelineState::Hasher>
        graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, RHIComputePipelineState> computePSCache;
    std::unordered_map<RHIRayTracingPipelineState::Key, RHIRayTracingPipelineState> rayTracingPSCache;
};

} // namespace vex