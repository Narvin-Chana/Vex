#include "RayTracingShaders.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

#define VEX_STRINGIFY(val) #val

#define VEX_INVALID_RT_SHADER_TYPE(name, value)                                                                        \
    VEX_LOG(Fatal, "Invalid type passed to TraceRays call for " VEX_STRINGIFY(name) " : {}", value)

namespace vex
{

void RayTracingCollection::ValidateShaderTypes(const RayTracingCollection& desc)
{
    for (const auto& rayGen : desc.rayGenerationShaders)
    {
        if (rayGen.type != ShaderType::RayGenerationShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayGenerationShader, rayGen.type);
        }
    }

    for (const auto& rayMiss : desc.rayMissShaders)
    {
        if (rayMiss.type != ShaderType::RayMissShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayMissShader, rayMiss.type);
        }
    }

    for (const auto& hitGroup : desc.hitGroups)
    {
        if (hitGroup.rayClosestHitShader.type != ShaderType::RayClosestHitShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayClosestHitShader, hitGroup.rayClosestHitShader.type);
        }

        if (hitGroup.rayAnyHitShader && hitGroup.rayAnyHitShader->type != ShaderType::RayAnyHitShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayAnyHitShader, hitGroup.rayAnyHitShader->type);
        }

        if (hitGroup.rayIntersectionShader && hitGroup.rayIntersectionShader->type != ShaderType::RayIntersectionShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayIntersectionShader, hitGroup.rayIntersectionShader->type);
        }
    }

    for (const auto& rayCallable : desc.rayCallableShaders)
    {
        if (rayCallable.type != ShaderType::RayCallableShader)
        {
            VEX_INVALID_RT_SHADER_TYPE(RayCallableShader, rayCallable.type);
        }
    }
}

} // namespace vex

#undef VEX_STRINGIFY
#undef VEX_INVALID_RT_SHADER_TYPE