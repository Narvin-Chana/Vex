#include "PipelineStateCache.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/Shader.h>
#include <Vex/ShaderResourceContext.h>

namespace vex
{

namespace PipelineStateCache_Internal
{

// Very annoying boilerplate code to validate shader versions between shader collection and a
// RHIRayTracingPipelineState.
static bool IsShaderCollectionStale(const RayTracingShaderCollection& shaderCollection,
                                    const RHIRayTracingPipelineState& rtPSO)
{
    static auto IsShaderVersionStale = [](const NonNullPtr<Shader> shader, u32 psVersion) -> bool
    { return shader->version > psVersion; };

    // Ray generation shader version check.
    if (IsShaderVersionStale(shaderCollection.rayGenerationShader, rtPSO.rayGenerationShaderVersion))
    {
        return true;
    }

    // Ray miss shaders version check.
    if (shaderCollection.rayMissShaders.size() != rtPSO.rayMissShaderVersions.size())
    {
        return true;
    }
    for (std::size_t i = 0; const NonNullPtr<Shader>& rayMissShader : shaderCollection.rayMissShaders)
    {
        if (IsShaderVersionStale(rayMissShader, rtPSO.rayMissShaderVersions[i]))
        {
            return true;
        }
        i++;
    }

    // HitGroup shaders version check.
    if (shaderCollection.hitGroupShaders.size() != rtPSO.hitGroupVersions.size())
    {
        return true;
    }
    for (std::size_t i = 0;
         const RayTracingShaderCollection::HitGroup& hitGroupShaders : shaderCollection.hitGroupShaders)
    {
        const auto& hitGroupVersions = rtPSO.hitGroupVersions[i];
        if (IsShaderVersionStale(hitGroupShaders.rayClosestHitShader, hitGroupVersions.rayClosestHitVersion))
        {
            return true;
        }

        static auto CheckHitGroupOptionalVersion = [](const std::optional<const NonNullPtr<Shader>>& shader,
                                                      const std::optional<u32>& psVersion) -> bool
        {
            if (shader.has_value() && !psVersion.has_value())
            {
                return true;
            }
            if (IsShaderVersionStale(shader.value(), psVersion.value()))
            {
                return true;
            }

            return false;
        };

        if (CheckHitGroupOptionalVersion(hitGroupShaders.rayAnyHitShader, hitGroupVersions.rayAnyHitVersion))
        {
            return true;
        }
        if (CheckHitGroupOptionalVersion(hitGroupShaders.rayIntersectionShader,
                                         hitGroupVersions.rayIntersectionVersion))
        {
            return true;
        }

        i++;
    }

    // Ray callable shaders version check.
    if (shaderCollection.rayCallableShaders.size() != rtPSO.rayCallableShaderVersions.size())
    {
        return true;
    }
    for (std::size_t i = 0; const NonNullPtr<Shader>& rayCallableShader : shaderCollection.rayCallableShaders)
    {
        if (IsShaderVersionStale(rayCallableShader, rtPSO.rayCallableShaderVersions[i]))
        {
            return true;
        }
        i++;
    }

    return false;
}

} // namespace PipelineStateCache_Internal

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

    const NonNullPtr<Shader> vertexShader = shaderCompiler.GetShader(ps.key.vertexShader, resourceContext);
    const NonNullPtr<Shader> pixelShader = shaderCompiler.GetShader(ps.key.pixelShader, resourceContext);
    if (!vertexShader->IsValid() || !pixelShader->IsValid())
    {
        return nullptr;
    }

    bool pipelineStateStale = false;
    pipelineStateStale |= vertexShader->version > ps.vertexShaderVersion;
    pipelineStateStale |= pixelShader->version > ps.pixelShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
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

    const NonNullPtr<Shader> computeShader = shaderCompiler.GetShader(ps.key.computeShader, resourceContext);
    if (!computeShader->IsValid())
    {
        return nullptr;
    }

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= computeShader->version > ps.computeShaderVersion;
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps.Cleanup(*resourceCleanup);
        ps.Compile(*computeShader, *resourceLayout);
    }

    return &ps;
}

std::optional<RayTracingShaderCollection> PipelineStateCache::GetRayTracingShaderCollection(
    const RHIRayTracingPipelineState::Key& key, ShaderResourceContext resourceContext)
{
    const NonNullPtr<Shader> rayGenerationShader = shaderCompiler.GetShader(key.rayGenerationShader, resourceContext);
    if (!rayGenerationShader->IsValid())
    {
        VEX_LOG(Error, "Unable to obtain valid rayGenerationShader: {}", key.rayGenerationShader);
        return std::nullopt;
    }
    RayTracingShaderCollection collection = RayTracingShaderCollection(rayGenerationShader);

    collection.rayMissShaders.reserve(key.rayMissShaders.size());
    for (const ShaderKey& key : key.rayMissShaders)
    {
        const NonNullPtr<Shader> rayMissShader = shaderCompiler.GetShader(key, resourceContext);
        if (!rayMissShader->IsValid())
        {
            VEX_LOG(Error, "Unable to obtain valid rayMissShader: {}", key);
            return std::nullopt;
        }
        collection.rayMissShaders.push_back(rayMissShader);
    }

    collection.hitGroupShaders.reserve(key.hitGroups.size());
    for (const HitGroup& hitGroup : key.hitGroups)
    {
        const NonNullPtr<Shader> rayClosestHitShader =
            shaderCompiler.GetShader(hitGroup.rayClosestHitShader, resourceContext);
        if (!rayClosestHitShader->IsValid())
        {
            VEX_LOG(Error, "Unable to obtain valid rayClosestHitShader: {}", hitGroup.rayClosestHitShader);
            return std::nullopt;
        }
        RayTracingShaderCollection::HitGroup hitGroupShaderCollection{ .rayClosestHitShader = rayClosestHitShader };

        if (hitGroup.rayAnyHitShader.has_value())
        {
            const NonNullPtr<Shader> rayAnyHitShader =
                shaderCompiler.GetShader(*hitGroup.rayAnyHitShader, resourceContext);
            if (!rayAnyHitShader->IsValid())
            {
                VEX_LOG(Error, "Unable to obtain valid rayAnyHitShader: {}", *hitGroup.rayAnyHitShader);
                return std::nullopt;
            }
            hitGroupShaderCollection.rayAnyHitShader.emplace(rayAnyHitShader);
        }

        if (hitGroup.rayIntersectionShader.has_value())
        {
            const NonNullPtr<Shader> rayIntersectionShader =
                shaderCompiler.GetShader(*hitGroup.rayIntersectionShader, resourceContext);
            if (!rayIntersectionShader->IsValid())
            {
                VEX_LOG(Error, "Unable to obtain valid rayIntersectionShader: {}", *hitGroup.rayIntersectionShader);
                return std::nullopt;
            }
            hitGroupShaderCollection.rayIntersectionShader.emplace(rayIntersectionShader);
        }

        collection.hitGroupShaders.push_back(std::move(hitGroupShaderCollection));
    }

    collection.rayCallableShaders.reserve(key.rayCallableShaders.size());
    for (const ShaderKey& key : key.rayCallableShaders)
    {
        const NonNullPtr<Shader> rayCallableShader = shaderCompiler.GetShader(key, resourceContext);
        if (!rayCallableShader->IsValid())
        {
            VEX_LOG(Error, "Unable to obtain valid rayCallableShader: {}", key);
            return std::nullopt;
        }
        collection.rayCallableShaders.push_back(rayCallableShader);
    }

    return collection;
}

const RHIRayTracingPipelineState* PipelineStateCache::GetRayTracingPipelineState(
    const RHIRayTracingPipelineState::Key& key, ShaderResourceContext resourceContext)
{
    const auto it = rayTracingPSCache.find(key);
    RHIRayTracingPipelineState& ps =
        it != rayTracingPSCache.end()
            ? it->second
            : rayTracingPSCache.insert({ key, rhi->CreateRayTracingPipelineState(key) }).first->second;

    // Add samplers to the resourceContext
    resourceContext.samplers = resourceLayout->GetStaticSamplers();

    std::optional<RayTracingShaderCollection> rtShaderCollection = GetRayTracingShaderCollection(key, resourceContext);
    if (!rtShaderCollection)
    {
        return nullptr;
    }

    // Recompile PSO if any associated data has changed.
    bool pipelineStateStale = false;
    pipelineStateStale |= PipelineStateCache_Internal::IsShaderCollectionStale(*rtShaderCollection, ps);
    pipelineStateStale |= resourceLayout->version > ps.rootSignatureVersion;
    if (pipelineStateStale)
    {
        // Avoids PSO being destroyed while frame is in flight.
        ps.Cleanup(*resourceCleanup);
        ps.Compile(std::move(*rtShaderCollection), *resourceLayout, *resourceCleanup);
    }

    return &ps;
}

ShaderCompiler& PipelineStateCache::GetShaderCompiler()
{
    return shaderCompiler;
}

} // namespace vex