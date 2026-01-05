#include "PipelineStateCache.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Utility/Validation.h>

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
            if (shader.has_value() && IsShaderVersionStale(shader.value(), psVersion.value()))
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

static void ValidateVertexInputLayoutOnShader(const Shader& shader, const VertexInputLayout& inputLayout)
{
    const ShaderReflection* reflection = shader.GetReflection();
    if (!reflection)
        return;

    //    VEX_CHECK(reflection->inputs.size() == inputLayout.attributes.size(),
    //              "Error validating shader {}: Incoherent vertex input layout: size doesnt match shader",
    //              shader.key);
    //
    //    for (u32 i = 0; i < reflection->inputs.size(); ++i)
    //    {
    //        VEX_CHECK(reflection->inputs[i].semanticName == inputLayout.attributes[i].semanticName,
    //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic name
    //                  " "doesn't match shader", shader.key, i);
    //        VEX_CHECK(reflection->inputs[i].semanticIndex == inputLayout.attributes[i].semanticIndex,
    //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic index
    //                  " "doesn't match shader", shader.key, i);
    //        VEX_CHECK(reflection->inputs[i].format == inputLayout.attributes[i].format,
    //                  "Error validating shader {}: Vertex input layout validation error: Attribute {}'s semantic index
    //                  " "doesn't match shader", shader.key, i);
    //    }
}

} // namespace PipelineStateCache_Internal

PipelineStateCache::PipelineStateCache(RHI* rhi,
                                       RHIDescriptorPool& descriptorPool,
                                       ResourceCleanup* resourceCleanup,
                                       const ShaderCompilerSettings& compilerSettings)
    : rhi(rhi)
    , resourceCleanup(resourceCleanup)
    , shaderCompiler(compilerSettings)
    , resourceLayout(rhi->CreateResourceLayout(descriptorPool))
{
}

PipelineStateCache::~PipelineStateCache() = default;

RHIResourceLayout& PipelineStateCache::GetResourceLayout()
{
    return *resourceLayout;
}

const RHIGraphicsPipelineState* PipelineStateCache::GetGraphicsPipelineState(const RHIGraphicsPipelineState::Key& key)
{
    VEX_CHECK(key.vertexShader.type == ShaderType::VertexShader,
              "Invalid ShaderType for vertex shader: {}",
              key.vertexShader.type);
    VEX_CHECK(key.pixelShader.type == ShaderType::PixelShader,
              "Invalid ShaderType for pixel shader: {}",
              key.pixelShader.type);

    const auto it = graphicsPSCache.find(key);
    RHIGraphicsPipelineState& ps =
        it != graphicsPSCache.end()
            ? it->second
            : graphicsPSCache.insert({ key, rhi->CreateGraphicsPipelineState(key) }).first->second;

    const NonNullPtr<Shader> vertexShader = shaderCompiler.GetShader(ps.key.vertexShader);
    PipelineStateCache_Internal::ValidateVertexInputLayoutOnShader(*vertexShader, key.vertexInputLayout);

    const NonNullPtr<Shader> pixelShader = shaderCompiler.GetShader(ps.key.pixelShader);
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

const RHIComputePipelineState* PipelineStateCache::GetComputePipelineState(const RHIComputePipelineState::Key& key)
{
    VEX_CHECK(key.computeShader.type == ShaderType::ComputeShader,
              "Invalid ShaderType for compute shader: {}",
              key.computeShader.type);

    const auto it = computePSCache.find(key);
    RHIComputePipelineState& ps =
        it != computePSCache.end() ? it->second
                                   : computePSCache.insert({ key, rhi->CreateComputePipelineState(key) }).first->second;

    const NonNullPtr<Shader> computeShader = shaderCompiler.GetShader(ps.key.computeShader);
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
    const RHIRayTracingPipelineState::Key& key)
{
    static auto ValidateShaderType = [](ShaderType expectedType, const ShaderKey& shaderKey)
    { VEX_CHECK(shaderKey.type == expectedType, "Invalid ShaderType for {}: {}", expectedType, shaderKey.type); };

    ValidateShaderType(ShaderType::RayGenerationShader, key.rayGenerationShader);
    const NonNullPtr<Shader> rayGenerationShader = shaderCompiler.GetShader(key.rayGenerationShader);
    if (!rayGenerationShader->IsValid())
    {
        VEX_LOG(Error, "Unable to obtain valid rayGenerationShader: {}", key.rayGenerationShader);
        return std::nullopt;
    }
    RayTracingShaderCollection collection = RayTracingShaderCollection(rayGenerationShader);

    collection.rayMissShaders.reserve(key.rayMissShaders.size());
    for (const ShaderKey& key : key.rayMissShaders)
    {
        ValidateShaderType(ShaderType::RayMissShader, key);
        const NonNullPtr<Shader> rayMissShader = shaderCompiler.GetShader(key);
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
        ValidateShaderType(ShaderType::RayClosestHitShader, hitGroup.rayClosestHitShader);
        const NonNullPtr<Shader> rayClosestHitShader = shaderCompiler.GetShader(hitGroup.rayClosestHitShader);
        if (!rayClosestHitShader->IsValid())
        {
            VEX_LOG(Error, "Unable to obtain valid rayClosestHitShader: {}", hitGroup.rayClosestHitShader);
            return std::nullopt;
        }
        RayTracingShaderCollection::HitGroup hitGroupShaderCollection{
            .name = hitGroup.name,
            .rayClosestHitShader = rayClosestHitShader,
        };

        if (hitGroup.rayAnyHitShader.has_value())
        {
            ValidateShaderType(ShaderType::RayAnyHitShader, *hitGroup.rayAnyHitShader);
            const NonNullPtr<Shader> rayAnyHitShader = shaderCompiler.GetShader(*hitGroup.rayAnyHitShader);
            if (!rayAnyHitShader->IsValid())
            {
                VEX_LOG(Error, "Unable to obtain valid rayAnyHitShader: {}", *hitGroup.rayAnyHitShader);
                return std::nullopt;
            }
            hitGroupShaderCollection.rayAnyHitShader.emplace(rayAnyHitShader);
        }

        if (hitGroup.rayIntersectionShader.has_value())
        {
            ValidateShaderType(ShaderType::RayIntersectionShader, *hitGroup.rayIntersectionShader);
            const NonNullPtr<Shader> rayIntersectionShader = shaderCompiler.GetShader(*hitGroup.rayIntersectionShader);
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
        ValidateShaderType(ShaderType::RayCallableShader, key);
        const NonNullPtr<Shader> rayCallableShader = shaderCompiler.GetShader(key);
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
    const RHIRayTracingPipelineState::Key& key, RHIAllocator& allocator)
{
    const auto it = rayTracingPSCache.find(key);
    RHIRayTracingPipelineState& ps =
        it != rayTracingPSCache.end()
            ? it->second
            : rayTracingPSCache.insert({ key, rhi->CreateRayTracingPipelineState(key) }).first->second;

    std::optional<RayTracingShaderCollection> rtShaderCollection = GetRayTracingShaderCollection(key);
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
        ps.Compile(std::move(*rtShaderCollection), *resourceLayout, *resourceCleanup, allocator);
    }

    return &ps;
}

ShaderCompiler& PipelineStateCache::GetShaderCompiler()
{
    return shaderCompiler;
}

} // namespace vex