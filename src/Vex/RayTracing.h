#pragma once

#include <string>

#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Types.h>

namespace vex
{

class Shader;

struct HitGroup
{
    std::string name;
    ShaderKey rayClosestHitShader;
    std::optional<ShaderKey> rayAnyHitShader;
    std::optional<ShaderKey> rayIntersectionShader;

    bool operator==(const HitGroup& other) const = default;
};

struct RayTracingPassDescription
{
    ShaderKey rayGenerationShader;
    std::vector<ShaderKey> rayMissShaders;
    std::vector<HitGroup> hitGroups;
    std::vector<ShaderKey> rayCallableShaders;

    // Max recursion of traced rays.
    u32 maxRecursionDepth;
    // Max size of ray payloads.
    u32 maxPayloadByteSize;
    // Max size of triangle attributes.
    u32 maxAttributeByteSize;

    bool operator==(const RayTracingPassDescription& other) const = default;

    static void ValidateShaderTypes(const RayTracingPassDescription& desc);
};

// Mirrored to RayTracingPassDescription, but with the actual shader objects instead of keys (for PSO compilation).
struct RayTracingShaderCollection
{
    RayTracingShaderCollection() = delete;
    RayTracingShaderCollection(NonNullPtr<Shader> rayGenerationShader)
        : rayGenerationShader(rayGenerationShader)
    {
    }
    RayTracingShaderCollection(const RayTracingShaderCollection&) = default;
    RayTracingShaderCollection(RayTracingShaderCollection&&) = default;
    RayTracingShaderCollection& operator=(const RayTracingShaderCollection&) = default;
    RayTracingShaderCollection& operator=(RayTracingShaderCollection&&) = default;

    NonNullPtr<Shader> rayGenerationShader;
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

VEX_MAKE_HASHABLE(vex::RayTracingPassDescription,
    VEX_HASH_COMBINE(seed, obj.rayGenerationShader);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayMissShaders);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.hitGroups);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.rayCallableShaders);
    VEX_HASH_COMBINE(seed, obj.maxRecursionDepth);
    VEX_HASH_COMBINE(seed, obj.maxPayloadByteSize);
    VEX_HASH_COMBINE(seed, obj.maxAttributeByteSize);
);

// clang-format on