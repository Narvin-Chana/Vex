#include "PipelineStateCache.h"

#include <Vex/Logger.h>
#include <Vex/RHI/RHI.h>
#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/RHI/RHIShader.h>

namespace vex
{

PipelineStateCache::PipelineStateCache(RHI* rhi,
                                       RHIDescriptorPool& descriptorPool,
                                       ResourceCleanup* resourceCleanup,
                                       const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , resourceCleanup(resourceCleanup)
    , shaderCompiler(rhi, compilerSettings)
    , resourceLayout(rhi->CreateResourceLayout(descriptorPool))
{
}

PipelineStateCache::~PipelineStateCache() = default;

RHIResourceLayout& PipelineStateCache::GetResourceLayout()
{
    return *resourceLayout;
}

const RHIGraphicsPipelineState* PipelineStateCache::GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key,
                                                                             ShaderResourceContext resourceContext)
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

    // Add samplers to the resourceContext
    resourceContext.samplers = resourceLayout->GetStaticSamplers();

    auto vertexShader = shaderCompiler.GetShader(ps->key.vertexShader, resourceContext);
    auto pixelShader = shaderCompiler.GetShader(ps->key.pixelShader, resourceContext);
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
                                                                           ShaderResourceContext resourceContext)
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

    // Add samplers to the resourceContext
    resourceContext.samplers = resourceLayout->GetStaticSamplers();

    auto shader = shaderCompiler.GetShader(ps->key.computeShader, resourceContext);
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

ShaderCompiler& PipelineStateCache::GetShaderCompiler()
{
    return shaderCompiler;
}

} // namespace vex