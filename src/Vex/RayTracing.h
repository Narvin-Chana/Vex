#pragma once

#include <vector>

#include <Vex/ShaderView.h>
#include <Vex/Types.h>

namespace vex
{

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
    ShaderView rayClosestHitShader;
    std::optional<ShaderView> rayAnyHitShader;
    std::optional<ShaderView> rayIntersectionShader;
};

struct RayTracingShaderCollection
{
    // Max recursion of traced rays.
    // 31 is the API defined max.
    u32 maxRecursionDepth = 31;
    // Max size of ray payloads.
    u32 maxPayloadByteSize;
    // Max size of triangle attributes.
    u32 maxAttributeByteSize;

    std::vector<ShaderView> rayGenerationShaders;
    std::vector<ShaderView> rayMissShaders;

    std::vector<HitGroup> hitGroups;
    std::vector<ShaderView> rayCallableShaders;
};

} // namespace vex
