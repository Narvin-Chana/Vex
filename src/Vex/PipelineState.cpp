#include "PipelineState.h"

#include <algorithm>

#include <Vex/RayTracing.h>
#include <Vex/ShaderView.h>
#include <Vex/Logger.h>
#include <VexMacros.h>

namespace vex
{

GraphicsPSOKey::GraphicsPSOKey(const DrawDesc& drawDesc, const RenderTargetState& renderTargetState)
    : name(std::format("VS: {}, PS: {}", drawDesc.vertexShader.name, drawDesc.pixelShader.name))
    , vertexShader(drawDesc.vertexShader.hash)
    , pixelShader(drawDesc.pixelShader.hash)
    , vertexInputLayout(drawDesc.vertexInputLayout)
    , inputAssembly(drawDesc.inputAssembly)
    , rasterizerState(drawDesc.rasterizerState)
    , depthStencilState(drawDesc.depthStencilState)
    , colorBlendState(drawDesc.colorBlendState)
    , renderTargetState(renderTargetState)
{
    VEX_CHECK(drawDesc.vertexShader.IsValid(), "Invalid shader for GraphicsPSO: {}", drawDesc.vertexShader.name);
    VEX_CHECK(drawDesc.pixelShader.IsValid(), "Invalid shader for GraphicsPSO: {}", drawDesc.pixelShader.name);
    VEX_CHECK(drawDesc.vertexShader.type == ShaderType::VertexShader,
              "Invalid ShaderType for vertex shader: {}",
              drawDesc.vertexShader.type);
    VEX_CHECK(drawDesc.pixelShader.type == ShaderType::PixelShader,
              "Invalid ShaderType for pixel shader: {}",
              drawDesc.pixelShader.type);
}

ComputePSOKey::ComputePSOKey(const ShaderView& computeShader)
    : name(computeShader.name)
    , computeShader(computeShader.hash)
{
    VEX_CHECK(computeShader.IsValid(), "Invalid shader for ComputePSO: {}", computeShader.name);
    VEX_CHECK(computeShader.type == ShaderType::ComputeShader,
              "Invalid ShaderType for compute shader: {}",
              computeShader.type);
}

RayTracingPSOKey::RayTracingPSOKey(const RayTracingShaderCollection& shaderCollection)
    : maxRecursionDepth(shaderCollection.maxRecursionDepth)
    , maxPayloadByteSize(shaderCollection.maxPayloadByteSize)
    , maxAttributeByteSize(shaderCollection.maxAttributeByteSize)
{
    static auto ValidateAndExtractHash = [](ShaderType expectedType)
    {
        return [expectedType](const ShaderView& shaderView)
        {
            VEX_CHECK(shaderView.IsValid(), "Shader error {}: Shader bytecode is empty!", shaderView.name);
            VEX_CHECK(shaderView.type == expectedType,
                      "Shader error {}: Invalid ShaderType for {}: {}",
                      shaderView.name,
                      expectedType,
                      shaderView.type);
            return shaderView.hash;
        };
    };

    rayGenerationShaders.reserve(shaderCollection.rayGenerationShaders.size());
    std::ranges::transform(shaderCollection.rayGenerationShaders,
                           std::back_inserter(rayGenerationShaders),
                           ValidateAndExtractHash(ShaderType::RayGenerationShader));
    rayMissShaders.reserve(shaderCollection.rayMissShaders.size());
    std::ranges::transform(shaderCollection.rayMissShaders,
                           std::back_inserter(rayMissShaders),
                           ValidateAndExtractHash(ShaderType::RayMissShader));
    hitGroups.reserve(shaderCollection.hitGroups.size());
    std::ranges::transform(
        shaderCollection.hitGroups,
        std::back_inserter(hitGroups),
        [](const vex::HitGroup& hitGroup) -> HitGroup
        {
            return { .name = hitGroup.name,
                     .rayClosestHitShader =
                         ValidateAndExtractHash(ShaderType::RayClosestHitShader)(hitGroup.rayClosestHitShader),
                     .rayAnyHitShader = hitGroup.rayAnyHitShader ? ValidateAndExtractHash(ShaderType::RayAnyHitShader)(
                                                                       *hitGroup.rayAnyHitShader)
                                                                 : std::optional<SHA1HashDigest>(),
                     .rayIntersectionShader = hitGroup.rayIntersectionShader
                                                  ? ValidateAndExtractHash(ShaderType::RayIntersectionShader)(
                                                        *hitGroup.rayIntersectionShader)
                                                  : std::optional<SHA1HashDigest>() };
        });
    rayCallableShaders.reserve(shaderCollection.rayCallableShaders.size());
    std::ranges::transform(shaderCollection.rayCallableShaders,
                           std::back_inserter(rayCallableShaders),
                           ValidateAndExtractHash(ShaderType::RayCallableShader));
}

} // namespace vex
