#pragma once

#include <vector>

#include <Vex/DrawHelpers.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Types.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

struct RayTracingShaderCollection;

struct GraphicsPSOKey
{
    GraphicsPSOKey(const DrawDesc& drawDesc, const RenderTargetState& renderTargetState);

    // Name used for identifying the PSO in graphics debuggers/error messages.
    std::string name;

    SHA1HashDigest vertexShader;
    SHA1HashDigest pixelShader;
    VertexInputLayout vertexInputLayout;
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

    // Name used for identifying the PSO in graphics debuggers/error messages.
    std::string name;

    SHA1HashDigest computeShader;
    constexpr bool operator==(const ComputePSOKey& other) const = default;
};

struct RayTracingPSOKey
{
    RayTracingPSOKey(const RayTracingShaderCollection& shaderCollection);

    // Name used for identifying the PSO in graphics debuggers/error messages.
    std::string name;

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

VEX_FORMATTABLE(vex::GraphicsPSOKey,
    "GraphicsPSO({}\n\tVSHash: \"{}\", PSHash: \"{}\")",
    obj.name,
    obj.vertexShader,
    obj.pixelShader
);

VEX_FORMATTABLE(vex::ComputePSOKey,
    "ComputePSO({}\n\tHash: \"{}\")",
    obj.name,
    obj.computeShader
);

// clang-format on