#pragma once

#include <string>
#include <vector>

#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>

namespace vex
{

class Shader;

struct TraceRaysDesc
{
    u32 width = 1, height = 1, depth = 1;
    u32 rayGenShaderIndex = 0;
    u32 rayMissShaderIndex = 0;
    u32 hitGroupShaderIndex = 0;
    u32 rayCallableShaderIndex = 0;
};

struct HitGroup
{
    std::string name;
    ShaderKey rayClosestHitShader;
    std::optional<ShaderKey> rayAnyHitShader;
    std::optional<ShaderKey> rayIntersectionShader;

    bool operator==(const HitGroup& other) const = default;
};

struct RayTracingCollection
{
    std::vector<ShaderKey> rayGenerationShaders;
    std::vector<ShaderKey> rayMissShaders;
    std::vector<HitGroup> hitGroups;
    std::vector<ShaderKey> rayCallableShaders;

    // Max recursion of traced rays.
    // 31 is the API defined max.
    u32 maxRecursionDepth = 31;
    // Max size of ray payloads.
    u32 maxPayloadByteSize;
    // Max size of triangle attributes.
    u32 maxAttributeByteSize;

    bool operator==(const RayTracingCollection& other) const = default;

    static void ValidateShaderTypes(const RayTracingCollection& desc);
};

// Mirrored to RayTracingCollection, but with the actual shader objects instead of keys (for PSO compilation).
struct RayTracingShaderCollection
{
    RayTracingShaderCollection() = default;
    RayTracingShaderCollection(const RayTracingShaderCollection&) = default;
    RayTracingShaderCollection(RayTracingShaderCollection&&) = default;
    RayTracingShaderCollection& operator=(const RayTracingShaderCollection&) = default;
    RayTracingShaderCollection& operator=(RayTracingShaderCollection&&) = default;

    std::vector<NonNullPtr<Shader>> rayGenerationShaders;
    std::vector<NonNullPtr<Shader>> rayMissShaders;

    struct HitGroup
    {
        std::string name;
        NonNullPtr<Shader> rayClosestHitShader;
        std::optional<NonNullPtr<Shader>> rayAnyHitShader;
        std::optional<NonNullPtr<Shader>> rayIntersectionShader;
    };
    std::vector<HitGroup> hitGroupShaders;
    std::vector<NonNullPtr<Shader>> rayCallableShaders;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::HitGroup, 
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.rayClosestHitShader);
    VEX_HASH_COMBINE(seed, obj.rayAnyHitShader);
    VEX_HASH_COMBINE(seed, obj.rayIntersectionShader);
);

VEX_MAKE_HASHABLE(vex::RayTracingCollection,
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayGenerationShaders);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayMissShaders);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.hitGroups);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayCallableShaders);
    VEX_HASH_COMBINE(seed, obj.maxRecursionDepth);
    VEX_HASH_COMBINE(seed, obj.maxPayloadByteSize);
    VEX_HASH_COMBINE(seed, obj.maxAttributeByteSize);
);

// clang-format on