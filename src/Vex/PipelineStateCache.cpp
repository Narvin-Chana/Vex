#include "PipelineStateCache.h"

#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/RHI/RHIShader.h>

namespace vex
{

PipelineStateCache::PipelineStateCache(RHI* rhi, const FeatureChecker& featureChecker)
    : rhi(rhi)
    , resourceLayout(rhi->CreateResourceLayout(featureChecker))
{
}

PipelineStateCache::~PipelineStateCache() = default;

RHIResourceLayout& PipelineStateCache::GetResourceLayout()
{
    return *resourceLayout;
}

const RHIGraphicsPipelineState* PipelineStateCache::GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key)
{
    RHIGraphicsPipelineState* ps;
    if (graphicsPSCache.contains(key))
    {
        ps = graphicsPSCache[key].get();
    }
    else
    {
        graphicsPSCache[key] = rhi->CreateGraphicsPipelineState(key);
        ps = graphicsPSCache[key].get();
    }

    auto vertexShader = GetShader(ps->key.vertexShader);
    auto pixelShader = GetShader(ps->key.pixelShader);

    bool pipelineStateStale = false;
    pipelineStateStale |= vertexShader->version > ps->vertexShaderVersion;
    pipelineStateStale |= pixelShader->version > ps->pixelShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps->rootSignatureVersion;

    if (pipelineStateStale)
    {
        ps->Compile(*vertexShader, *pixelShader, *resourceLayout);
    }

    return ps;
}

const RHIComputePipelineState* PipelineStateCache::GetComputePipelineState(const RHIComputePipelineState::Key& key)
{
    RHIComputePipelineState* ps;
    if (computePSCache.contains(key))
    {
        ps = computePSCache[key].get();
    }
    else
    {
        computePSCache[key] = rhi->CreateComputePipelineState(key);
        ps = computePSCache[key].get();
    }

    auto shader = GetShader(ps->key.computeShader);

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= shader->version > ps->computeShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps->rootSignatureVersion;

    if (pipelineStateStale)
    {
        ps->Compile(*shader, *resourceLayout);
    }

    return ps;
}

const RHIShader* PipelineStateCache::GetShader(const ShaderKey& key)
{
    RHIShader* shader;
    if (shaderCache.contains(key))
    {
        shader = shaderCache[key].get();
    }
    else
    {
        shaderCache[key] = rhi->CreateShader(key);
        shader = shaderCache[key].get();
    }

    return shader;
}

} // namespace vex