#include "PipelineState.h"

#include <algorithm>

#include <Vex/Logger.h>
#include <Vex/RayTracing.h>
#include <Vex/ShaderView.h>
#include <VexMacros.h>

namespace vex
{

namespace PipelineState_Internal
{
// Appends " | <label>: <value>" to the name.
static void AppendLabelValueSection(std::string& name, std::string_view label, std::string_view value)
{
    std::format_to(std::back_inserter(name), " | {}: {}", label, value);
}

// Appends " #<8 hex digits>" to the name and returns it.
static std::string FinalizeName(std::string name, std::size_t keyHash)
{
    std::format_to(std::back_inserter(name), " #{:08x}", static_cast<u32>(keyHash));
    return name;
}

static std::string FormatColorFormats(const RenderTargetState& renderTargetState)
{
    std::string formats;
    for (const auto& [format, isSRGB] : renderTargetState.colorFormats)
    {
        std::format_to(std::back_inserter(formats),
                       "{}{}{}",
                       formats.empty() ? "" : ", ",
                       format,
                       isSRGB ? "_SRGB" : "");
    }
    return formats;
}
} // namespace PipelineState_Internal

std::string PSOUtil::GetGraphicsPSOName(const DrawDesc& drawDesc,
                                        const RenderTargetState& renderTargetState,
                                        std::size_t keyHash)
{
    using namespace PipelineState_Internal;
    std::string name = "Graphics";
    AppendLabelValueSection(name, "VS", drawDesc.vertexShader.name);
    AppendLabelValueSection(name, "PS", drawDesc.pixelShader.name);
    if (!renderTargetState.colorFormats.empty())
    {
        AppendLabelValueSection(name, "RT", FormatColorFormats(renderTargetState));
    }
    if (renderTargetState.depthStencilFormat != TextureFormat::UNKNOWN)
    {
        AppendLabelValueSection(name, "DS", magic_enum::enum_name(renderTargetState.depthStencilFormat));
    }
    return FinalizeName(std::move(name), keyHash);
}

std::string PSOUtil::GetComputePSOName(const ShaderView& computeShader, std::size_t keyHash)
{
    using namespace PipelineState_Internal;
    std::string name = "Compute";
    AppendLabelValueSection(name, "CS", computeShader.name);
    return FinalizeName(std::move(name), keyHash);
}

std::string PSOUtil::GetRayTracingPSOName(const RayTracingShaderCollection& shaderCollection, std::size_t keyHash)
{
    using namespace PipelineState_Internal;
    std::string name = "RayTracing";

    if (!shaderCollection.rayGenerationShaders.empty())
    {
        std::string rayGenerationShaders;
        for (const ShaderView& shader : shaderCollection.rayGenerationShaders)
        {
            rayGenerationShaders += rayGenerationShaders.empty() ? "" : ", ";
            rayGenerationShaders += shader.name;
        }
        AppendLabelValueSection(name, "RGen", rayGenerationShaders);
    }

    // Miss and callable shaders are only counted to keep the name short, hit groups are listed as they are named.
    if (!shaderCollection.rayMissShaders.empty())
    {
        AppendLabelValueSection(name, "Miss", std::to_string(shaderCollection.rayMissShaders.size()));
    }

    if (!shaderCollection.hitGroups.empty())
    {
        std::string hitGroups;
        for (const HitGroup& hitGroup : shaderCollection.hitGroups)
        {
            hitGroups += hitGroups.empty() ? "" : ", ";
            hitGroups += hitGroup.name;
        }
        AppendLabelValueSection(name, "Hit", hitGroups);
    }

    if (!shaderCollection.rayCallableShaders.empty())
    {
        AppendLabelValueSection(name, "Callable", std::to_string(shaderCollection.rayCallableShaders.size()));
    }

    AppendLabelValueSection(name,
                            "Recursion",
                            std::format("{}, Payload: {}B, Attr: {}B",
                                        shaderCollection.maxRecursionDepth,
                                        shaderCollection.maxPayloadByteSize,
                                        shaderCollection.maxAttributeByteSize));

    return FinalizeName(std::move(name), keyHash);
}

GraphicsPSOKey::GraphicsPSOKey(const DrawDesc& drawDesc, const RenderTargetState& renderTargetState)
    : vertexShader(drawDesc.vertexShader.hash)
    , pixelShader(drawDesc.pixelShader.hash)
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
    : computeShader(computeShader.hash)
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
    static constexpr auto ValidateAndExtractHash = [](ShaderType expectedType)
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
