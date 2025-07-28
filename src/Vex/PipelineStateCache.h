#pragma once

#include <span>
#include <unordered_map>

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/RHI/RHIPipelineState.h>
#include <Vex/ShaderCompiler.h>
#include <Vex/ShaderResourceContext.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct ShaderKey;
class FeatureChecker;

class PipelineStateCache
{
public:
    PipelineStateCache() = default;
    PipelineStateCache(RHI* rhi,
                       RHIDescriptorPool& descriptorPool,
                       const FeatureChecker& featureChecker,
                       ResourceCleanup* resourceCleanup,
                       bool enableShaderDebugging);
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

    ShaderCache& GetShaderCache();

private:
    RHI* rhi;

    ResourceCleanup* resourceCleanup;

    ShaderCache shaderCache;
    UniqueHandle<RHIResourceLayout> resourceLayout;
    std::unordered_map<RHIGraphicsPipelineState::Key, UniqueHandle<RHIGraphicsPipelineState>> graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, UniqueHandle<RHIComputePipelineState>> computePSCache;
};

} // namespace vex