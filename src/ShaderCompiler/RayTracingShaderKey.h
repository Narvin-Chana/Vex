#pragma once

#include <string>
#include <vector>

#include <Vex/Utility/NonNullPtr.h>

#include <ShaderCompiler/ShaderKey.h>

namespace vex::sc
{

struct HitGroupKey
{
    std::string name;
    ShaderKey rayClosestHitShader;
    std::optional<ShaderKey> rayAnyHitShader;
    std::optional<ShaderKey> rayIntersectionShader;

    constexpr bool operator==(const HitGroupKey& other) const = default;
};

struct RayTracingShaderKey
{
    // Max recursion of traced rays.
    // 31 is the API defined max.
    u32 maxRecursionDepth = 31;
    // Max size of ray payloads.
    u32 maxPayloadByteSize;
    // Max size of triangle attributes.
    u32 maxAttributeByteSize;

    std::vector<ShaderKey> rayGenerationShaders;
    std::vector<ShaderKey> rayMissShaders;
    std::vector<HitGroupKey> hitGroups;
    std::vector<ShaderKey> rayCallableShaders;

    constexpr bool operator==(const RayTracingShaderKey& other) const = default;

    static void ValidateShaderTypes(const RayTracingShaderKey& desc);
};

} // namespace vex::sc
