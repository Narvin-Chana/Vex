#pragma once

#include <unordered_map>

#include <Vex/RHIFwd.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/ShaderCompiler.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class FeatureChecker;

class PipelineStateCache
{
public:
    PipelineStateCache() = default;
    PipelineStateCache(RHI* rhi,
                       RHIDescriptorPool& descriptorPool,
                       ResourceCleanup* resourceCleanup,
                       const ShaderCompilerSettings& compilerSettings);
    ~PipelineStateCache();

    PipelineStateCache(const PipelineStateCache&) = delete;
    PipelineStateCache& operator=(const PipelineStateCache&) = delete;

    PipelineStateCache(PipelineStateCache&&) = default;
    PipelineStateCache& operator=(PipelineStateCache&&) = default;

    RHIResourceLayout& GetResourceLayout();

    const RHIGraphicsPipelineState* GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key,
                                                             ShaderResourceContext resourceContext);
    const RHIComputePipelineState* GetComputePipelineState(const RHIComputePipelineState::Key& key,
                                                           ShaderResourceContext resourceContext);

    ShaderCompiler& GetShaderCompiler();

private:
    RHI* rhi;

    ResourceCleanup* resourceCleanup;

    ShaderCompiler shaderCompiler;
    UniqueHandle<RHIResourceLayout> resourceLayout;
    std::unordered_map<RHIGraphicsPipelineState::Key, RHIGraphicsPipelineState, RHIGraphicsPipelineState::Hasher>
        graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, RHIComputePipelineState> computePSCache;
};

} // namespace vex