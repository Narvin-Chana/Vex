#pragma once

#include <unordered_map>

#include <Vex/RHI/RHIFwd.h>
#include <Vex/RHI/RHIPipelineState.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct ShaderKey;
class FeatureChecker;

class PipelineStateCache
{
public:
    PipelineStateCache() = default;
    PipelineStateCache(RHI* rhi, const FeatureChecker& featureChecker);
    ~PipelineStateCache();

    PipelineStateCache(const PipelineStateCache&) = delete;
    PipelineStateCache& operator=(const PipelineStateCache&) = delete;

    PipelineStateCache(PipelineStateCache&&) = default;
    PipelineStateCache& operator=(PipelineStateCache&&) = default;

    RHIResourceLayout& GetResourceLayout();

    const RHIGraphicsPipelineState* GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key);
    const RHIComputePipelineState* GetComputePipelineState(const RHIComputePipelineState::Key& key);

    const RHIShader* GetShader(const ShaderKey& key);

private:
    RHI* rhi;

    UniqueHandle<RHIResourceLayout> resourceLayout;

    std::unordered_map<RHIGraphicsPipelineState::Key, UniqueHandle<RHIGraphicsPipelineState>> graphicsPSCache;
    std::unordered_map<RHIComputePipelineState::Key, UniqueHandle<RHIComputePipelineState>> computePSCache;

    std::unordered_map<ShaderKey, UniqueHandle<RHIShader>> shaderCache;
};

} // namespace vex