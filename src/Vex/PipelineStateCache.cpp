#include "PipelineStateCache.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RayTracing.h>
#include <Vex/ShaderView.h>
#include <VexMacros.h>

namespace vex
{

PipelineStateCache::PipelineStateCache(NonNullPtr<RHI> rhi, RHIDescriptorPool& descriptorPool)
    : resourceLayout(rhi->CreateResourceLayout(descriptorPool))
    , rhi(rhi)
{
}

PipelineStateCache::~PipelineStateCache() = default;

RHIGraphicsPipelineState* PipelineStateCache::GetGraphicsPipelineState(
    const DrawDesc& drawDesc,
    const RenderTargetState& renderTargetState,
    std::unique_ptr<RHIGraphicsPipelineState>& oldPSO)
{
    if (drawDesc.vertexShader.IsErrored() || drawDesc.pixelShader.IsErrored())
    {
        return nullptr;
    }

    GraphicsPSOKey key{ drawDesc, renderTargetState };
    auto it = graphicsPSCache.find(key);
    if (it == graphicsPSCache.end())
    {
        std::string psName =
            PSOUtil::GetGraphicsPSOName(drawDesc, renderTargetState, graphicsPSCache.hash_function()(key));
        it = graphicsPSCache.emplace(key, rhi->CreateGraphicsPipelineState(std::move(psName), key)).first;
    }
    RHIGraphicsPipelineState& ps = it->second;

    bool pipelineStateStale = false;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoid PSO being destroyed while frame is in flight.
        oldPSO = ps.Cleanup();
        ps.Compile(drawDesc.vertexShader, drawDesc.pixelShader, *resourceLayout);
    }

    return &ps;
}

RHIComputePipelineState* PipelineStateCache::GetComputePipelineState(const ShaderView& computeShader,
                                                                     std::unique_ptr<RHIComputePipelineState>& oldPSO)
{
    if (computeShader.IsErrored())
    {
        return nullptr;
    }

    ComputePSOKey key{ computeShader };
    auto it = computePSCache.find(key);
    if (it == computePSCache.end())
    {
        std::string psName = PSOUtil::GetComputePSOName(computeShader, computePSCache.hash_function()(key));
        it = computePSCache.emplace(key, rhi->CreateComputePipelineState(std::move(psName), key)).first;
    }
    RHIComputePipelineState& ps = it->second;

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        oldPSO = ps.Cleanup();
        ps.Compile(computeShader, *resourceLayout);
    }

    return &ps;
}

RHIRayTracingPipelineState* PipelineStateCache::GetRayTracingPipelineState(
    const RayTracingShaderCollection& shaderCollection,
    RHIAllocator& allocator,
    std::unique_ptr<RHIRayTracingPipelineState>& oldPSO,
    std::vector<MaybeUninitialized<RHIBuffer>>& oldBuffers)
{
    if (std::ranges::any_of(shaderCollection.rayGenerationShaders,
                            [](const ShaderView& view) { return view.IsErrored(); }) ||
        std::ranges::any_of(shaderCollection.rayMissShaders, [](const ShaderView& view) { return view.IsErrored(); }) ||
        std::ranges::any_of(shaderCollection.hitGroups,
                            [](const HitGroup& hg)
                            {
                                if (hg.rayIntersectionShader && hg.rayIntersectionShader->IsErrored())
                                    return true;
                                if (hg.rayAnyHitShader && hg.rayAnyHitShader->IsErrored())
                                    return true;
                                return hg.rayClosestHitShader.IsErrored();
                            }) ||
        std::ranges::any_of(shaderCollection.rayCallableShaders,
                            [](const ShaderView& view) { return view.IsErrored(); }))
    {
        return nullptr;
    }

    RayTracingPSOKey key{ shaderCollection };
    auto it = rayTracingPSCache.find(key);
    if (it == rayTracingPSCache.end())
    {
        std::string psName = PSOUtil::GetRayTracingPSOName(shaderCollection, rayTracingPSCache.hash_function()(key));
        it = rayTracingPSCache.emplace(key, rhi->CreateRayTracingPipelineState(std::move(psName), key)).first;
    }

    RHIRayTracingPipelineState& ps = it->second;
    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        oldPSO = ps.Cleanup();
        oldBuffers = ps.Compile(shaderCollection, *resourceLayout, allocator);
    }

    return &ps;
}

} // namespace vex