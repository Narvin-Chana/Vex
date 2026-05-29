#include "PipelineStateCache.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RayTracing.h>
#include <Vex/ShaderView.h>
#include <VexMacros.h>

namespace vex
{

namespace PipelineStateCache_Internal
{

// static void ValidateVertexInputLayoutOnShader(const Shader& shader, const VertexInputLayout& inputLayout)
// {
//     const ShaderReflection* reflection = shader.GetReflection();
//     if (!reflection)
//         return;
//
//     // TODO(https://trello.com/c/C9bfFI8s): Fix issue with shader validation reflection crashing when the shader has
//     // optimized-out SV_ parameters (eg: unused vertex channels)
//
//     //    VEX_CHECK(reflection->inputs.size() == inputLayout.attributes.size(),
//     //              "Error validating shader {}: Incoherent vertex input layout: size doesnt match shader",
//     //              shader.key);
//     //
//     //    for (u32 i = 0; i < reflection->inputs.size(); ++i)
//     //    {
//     //        VEX_CHECK(reflection->inputs[i].semanticName == inputLayout.attributes[i].semanticName,
//     //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic
//     name
//     //                  " "doesn't match shader", shader.key, i);
//     //        VEX_CHECK(reflection->inputs[i].semanticIndex == inputLayout.attributes[i].semanticIndex,
//     //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic
//     index
//     //                  " "doesn't match shader", shader.key, i);
//     //        VEX_CHECK(reflection->inputs[i].format == inputLayout.attributes[i].format,
//     //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic
//     index
//     //                  " "doesn't match shader", shader.key, i);
//     //    }
// }

} // namespace PipelineStateCache_Internal

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
    const auto it = graphicsPSCache.find(key);
    RHIGraphicsPipelineState& ps =
        it != graphicsPSCache.end()
            ? it->second
            : graphicsPSCache.insert({ key, rhi->CreateGraphicsPipelineState(key) }).first->second;

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
    const auto it = computePSCache.find(key);
    RHIComputePipelineState& ps =
        it != computePSCache.end() ? it->second
                                   : computePSCache.insert({ key, rhi->CreateComputePipelineState(key) }).first->second;

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
    const auto it = rayTracingPSCache.find(key);
    RHIRayTracingPipelineState& ps =
        it != rayTracingPSCache.end()
            ? it->second
            : rayTracingPSCache.insert({ key, rhi->CreateRayTracingPipelineState(key) }).first->second;

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