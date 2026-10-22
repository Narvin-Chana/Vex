#pragma once

#include <optional>
#include <vector>

#include <Vex/DrawHelpers.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

struct RayTracingShaderCollection;

struct PSOUtil
{
    // We avoid using direct std::formatter specializations for these as their data is complex and never stored inside
    // one singular object. It is easier to instead just have a few free functions for this purpose.

    static std::string GetGraphicsPSOName(const DrawDesc& drawDesc,
                                          const RenderTargetState& renderTargetState,
                                          std::size_t keyHash);
    static std::string GetGraphicsPSOName(const DispatchMeshDesc& drawDesc,
                                          const RenderTargetState& renderTargetState,
                                          std::size_t keyHash);
    static std::string GetComputePSOName(const ShaderView& computeShader, std::size_t keyHash);
    static std::string GetRayTracingPSOName(const RayTracingShaderCollection& shaderCollection, std::size_t keyHash);
};

struct GraphicsPSOKey
{
    GraphicsPSOKey(const DrawDesc& drawDesc, const RenderTargetState& renderTargetState);
    GraphicsPSOKey(const DispatchMeshDesc& drawDesc, const RenderTargetState& renderTargetState);

    SHA1HashDigest vertexShader;
    SHA1HashDigest meshShader;
    SHA1HashDigest amplificationShader;
    SHA1HashDigest pixelShader;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
    RenderTargetState renderTargetState;

    constexpr bool operator==(const GraphicsPSOKey& other) const = default;
};

struct ComputePSOKey
{
    ComputePSOKey(const ShaderView& computeShader);

    SHA1HashDigest computeShader;
    constexpr bool operator==(const ComputePSOKey& other) const = default;
};

struct RayTracingPSOKey
{
    RayTracingPSOKey(const RayTracingShaderCollection& shaderCollection);

    // Max recursion of traced rays.
    // 31 is the API defined max.
    u32 maxRecursionDepth = 31;
    // Max size of ray payloads.
    u32 maxPayloadByteSize;
    // Max size of triangle attributes.
    u32 maxAttributeByteSize;

    // ---
    // Shader hashes
    // ---

    std::vector<SHA1HashDigest> rayGenerationShaders;
    std::vector<SHA1HashDigest> rayMissShaders;
    struct HitGroup
    {
        std::string name;
        SHA1HashDigest rayClosestHitShader;
        std::optional<SHA1HashDigest> rayAnyHitShader;
        std::optional<SHA1HashDigest> rayIntersectionShader;

        constexpr bool operator==(const HitGroup&) const = default;
    };
    std::vector<HitGroup> hitGroups;
    std::vector<SHA1HashDigest> rayCallableShaders;

    // ---

    constexpr bool operator==(const RayTracingPSOKey&) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::ComputePSOKey,
    VEX_HASH_COMBINE(seed, obj.computeShader);
);

VEX_MAKE_HASHABLE(vex::RayTracingPSOKey::HitGroup,
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.rayClosestHitShader);
    VEX_HASH_COMBINE(seed, obj.rayAnyHitShader);
    VEX_HASH_COMBINE(seed, obj.rayIntersectionShader);
);

VEX_MAKE_HASHABLE(vex::RayTracingPSOKey,
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayGenerationShaders);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayMissShaders);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.hitGroups);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayCallableShaders);
    VEX_HASH_COMBINE(seed, obj.maxRecursionDepth);
    VEX_HASH_COMBINE(seed, obj.maxPayloadByteSize);
    VEX_HASH_COMBINE(seed, obj.maxAttributeByteSize);
);

// clang-format on