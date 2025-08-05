#include "PipelineStateCache.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/Shader.h>
#include <Vex/ShaderResourceContext.h>

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
    const auto it = graphicsPSCache.find(key);
    RHIGraphicsPipelineState& ps =
        it != graphicsPSCache.end()
            ? it->second
            : graphicsPSCache.insert({ key, rhi->CreateGraphicsPipelineState(key) }).first->second;

    // Add samplers to the resourceContext
    resourceContext.samplers = resourceLayout->GetStaticSamplers();

    auto vertexShader = shaderCompiler.GetShader(ps.key.vertexShader, resourceContext);
    auto pixelShader = shaderCompiler.GetShader(ps.key.pixelShader, resourceContext);
    if (!vertexShader->IsValid() || !pixelShader->IsValid())
    {
        return nullptr;
    }

    bool pipelineStateStale = false;
    pipelineStateStale |= vertexShader->version > ps.vertexShaderVersion;
    pipelineStateStale |= pixelShader->version > ps.pixelShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    // TODO(https://trello.com/c/qdCLRSHl): add other fields, maybe via a custom == func?
    // Or just accept new slot in the cache?
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps.Cleanup(*resourceCleanup);
        ps.Compile(*vertexShader, *pixelShader, *resourceLayout);
    }

    return &ps;
}

const RHIComputePipelineState* PipelineStateCache::GetComputePipelineState(const RHIComputePipelineState::Key& key,
                                                                           ShaderResourceContext resourceContext)
{
    const auto it = computePSCache.find(key);
    RHIComputePipelineState& ps =
        it != computePSCache.end() ? it->second
                                   : computePSCache.insert({ key, rhi->CreateComputePipelineState(key) }).first->second;

    // Add samplers to the resourceContext
    resourceContext.samplers = resourceLayout->GetStaticSamplers();

    const auto shader = shaderCompiler.GetShader(ps.key.computeShader, resourceContext);
    if (!shader->IsValid())
    {
        return nullptr;
    }

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= shader->version > ps.computeShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps.Cleanup(*resourceCleanup);
        ps.Compile(*shader, *resourceLayout);
    }

    return &ps;
}

ShaderCompiler& PipelineStateCache::GetShaderCompiler()
{
    return shaderCompiler;
}

} // namespace vex