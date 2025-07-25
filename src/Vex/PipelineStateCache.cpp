#include "PipelineStateCache.h"

#include <Vex/Logger.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/RHI/RHIShader.h>

namespace vex
{

PipelineStateCache::PipelineStateCache(RHI* rhi,
                                       RHIDescriptorPool& descriptorPool,
                                       const FeatureChecker& featureChecker,
                                       ResourceCleanup* resourceCleanup,
                                       bool enableShaderDebugging)
    : rhi(rhi)
    , resourceCleanup(resourceCleanup)
    , shaderCache(rhi, enableShaderDebugging)
    , resourceLayout(rhi->CreateResourceLayout(featureChecker, descriptorPool))
{
}

PipelineStateCache::~PipelineStateCache() = default;

RHIResourceLayout& PipelineStateCache::GetResourceLayout()
{
    return *resourceLayout;
}

const RHIGraphicsPipelineState* PipelineStateCache::GetGraphicsPipelineState(
    const RHIGraphicsPipelineState::Key& key, const ShaderResourceContext& resourceContext)
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

    auto vertexShader = shaderCache.GetShader(ps->key.vertexShader, resourceContext);
    auto pixelShader = shaderCache.GetShader(ps->key.pixelShader, resourceContext);
    if (!vertexShader->IsValid() || !pixelShader->IsValid())
    {
        return nullptr;
    }

    bool pipelineStateStale = false;
    pipelineStateStale |= vertexShader->version > ps->vertexShaderVersion;
    pipelineStateStale |= pixelShader->version > ps->pixelShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps->rootSignatureVersion;
    // TODO: add other fields, maybe via custom == func? Or just accept new slot in cache?
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps->Cleanup(*resourceCleanup);
        ps->Compile(*vertexShader, *pixelShader, *resourceLayout);
    }

    return ps;
}

const RHIComputePipelineState* PipelineStateCache::GetComputePipelineState(const RHIComputePipelineState::Key& key,
                                                                           const ShaderResourceContext& resourceContext)
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

    auto shader = shaderCache.GetShader(ps->key.computeShader, resourceContext);
    if (!shader->IsValid())
    {
        return nullptr;
    }

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= shader->version > ps->computeShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps->rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps->Cleanup(*resourceCleanup);
        ps->Compile(*shader, *resourceLayout);
    }

    return ps;
}

ShaderCache& PipelineStateCache::GetShaderCache()
{
    return shaderCache;
}

} // namespace vex