#include <Vex.hlsli>

struct UniformStruct
{
    uint accelerationStructureHandle;
    uint outputBufferHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static RaytracingAccelerationStructure AccelerationStructure = GetBindlessResource(Uniforms.accelerationStructureHandle);
static RWStructuredBuffer<float3> Output = GetBindlessResource(Uniforms.outputBufferHandle);

struct HitAttributes
{
    float2 barycentrics;
};

struct [raypayload] RayPayload
{
    float hitValue : write(closesthit, miss) : read(caller);
    int depth : write(caller) : read(closesthit);
};

struct [raypayload] ShadowPayload
{
    float visibility : write(miss) : read(caller);
};

struct CallableData
{
    float result;
};

#ifndef HIT_GROUP_OFFSET
#define HIT_GROUP_OFFSET 0
#endif

///////////////////////////
// Ray Generation Shaders
///////////////////////////

// Fires a primary ray using miss index 0 and hit group offset 0.
// Used by most compile-verification tests.
[shader("raygeneration")]
void RayGenBasicMain()
{
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.depth = 0;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        HIT_GROUP_OFFSET, // hit group offset - overridden via define
        1, // geometry contribution multiplier
        0, // miss shader index 0
        ray,
        payload
    );

    Output[0] = payload.hitValue;
}

#ifndef HIT_GROUP_OFFSET
#define HIT_GROUP_OFFSET 0
#endif

// Fires a primary ray and a shadow ray (miss index 1), then invokes callable 0.
// Used by CompilePipeline_WithCallableShaders and SBT_SelectDifferentMissShaders.
[shader("raygeneration")]
void RayGenMain()
{
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.depth = 0;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        0, // hit group offset
        1,
        0, // miss shader index 0
        ray,
        payload
    );

    float result = payload.hitValue;

    // Always fire the shadow ray so miss shader index 1 is unconditionally exercised.
    ShadowPayload shadowPayload;

    RayDesc shadowRay;
    shadowRay.Origin = float3(0, 0, 0);
    shadowRay.Direction = normalize(float3(1, 1, 0));
    shadowRay.TMin = 0.001f;
    shadowRay.TMax = 10000.0f;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xFF,
        0,
        1,
        1, // miss shader index 1 - always invoked for a ray fired away from geometry
        shadowRay,
        shadowPayload
    );

    result += shadowPayload.visibility;

    // Invoke callable shader 0.
    CallableData callableData;
    CallShader(0, callableData);
    result += callableData.result;

    Output[0] = result;
}

// Alternative raygen - fires from a different origin using callable 1.
// Used by SBT_SelectDifferentRayGenShaders (index 1).
[shader("raygeneration")]
void RayGenMainAlt()
{
    RayDesc ray;
    ray.Origin = float3(1, 1, 1);
    ray.Direction = float3(0, 0, -1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    RayPayload payload;
    payload.depth = 0;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        ray,
        payload
    );

    float result = payload.hitValue;

    // Invoke callable shader 1.
    CallableData callableData;
    CallShader(1, callableData);
    result += callableData.result;

    Output[0] = result;
}

// Fires two rays: one with RayContributionToHitGroupIndex=0 (HitGroup1),
// one with RayContributionToHitGroupIndex=1 (HitGroup2).
// Used by SBT_SelectDifferentHitGroups and CompilePipeline_MultipleHitGroups.
[shader("raygeneration")]
void RayGenHitGroupSelectionMain()
{
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    // --- Hit group 0 ---
    RayPayload payload0;
    payload0.depth = 0;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        0, // RayContributionToHitGroupIndex → selects HitGroup1
        1,
        0,
        ray,
        payload0
    );

    // --- Hit group 1 ---
    RayPayload payload1;
    payload1.depth = 0;

    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        1, // RayContributionToHitGroupIndex → selects HitGroup2
        1,
        0,
        ray,
        payload1
    );

    Output[0] = float3(payload0.hitValue, payload1.hitValue, 0);
}

///////////////////////////
// Miss Shaders
///////////////////////////

[shader("miss")]
void MissMain(inout RayPayload payload)
{
    payload.hitValue = -1;
}

[shader("miss")]
void MissShadow(inout ShadowPayload payload)
{
    payload.visibility = 1.0f;
}

///////////////////////////
// Closest Hit Shaders
///////////////////////////

[shader("closesthit")]
void ClosestHitMain(inout RayPayload payload, in HitAttributes attrib)
{
    payload.hitValue = 1;

    if (payload.depth < 3)
    {
        RayDesc reflectionRay;
        reflectionRay.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        reflectionRay.Direction = reflect(WorldRayDirection(), float3(0, 1, 0));
        reflectionRay.TMin = 0.001f;
        reflectionRay.TMax = 10000.0f;

        RayPayload reflectionPayload;
        reflectionPayload.depth = payload.depth + 1;

        TraceRay(
            AccelerationStructure,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            reflectionRay,
            reflectionPayload
        );

        float reflectionResult = reflectionPayload.hitValue;
    }
}

[shader("closesthit")]
void ClosestHitMainAlt(inout RayPayload payload, in HitAttributes attrib)
{
    payload.hitValue = 2;

    if (payload.depth < 2)
    {
        RayDesc reflectionRay;
        reflectionRay.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        reflectionRay.Direction = reflect(WorldRayDirection(), float3(1, 0, 0));
        reflectionRay.TMin = 0.001f;
        reflectionRay.TMax = 10000.0f;

        RayPayload reflectionPayload;
        reflectionPayload.depth = payload.depth + 1;

        TraceRay(
            AccelerationStructure,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            reflectionRay,
            reflectionPayload
        );

        float reflectionResult = reflectionPayload.hitValue;
    }
}

///////////////////////////
// Any Hit Shaders
///////////////////////////

[shader("anyhit")]
void AnyHitMain(inout RayPayload payload, in HitAttributes attrib)
{
}

///////////////////////////
// Callable Shaders
///////////////////////////

[shader("callable")]
void CallableMain(inout CallableData data)
{
    data.result = 1.0f;
}

[shader("callable")]
void CallableMainAlt(inout CallableData data)
{
    data.result = 0.5f;
}
