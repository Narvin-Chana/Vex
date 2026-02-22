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

///////////////////////////
// Ray Generation Shaders
///////////////////////////

[shader("raygeneration")]
void RayGenBasicMain()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;
    
    // Primary ray
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;
    
    RayPayload payload;
    payload.depth = 0;
    
#ifndef HIT_GROUP_OFFSET
#define HIT_GROUP_OFFSET 0
#endif
    
    // Trace primary ray - will invoke miss or closest hit
    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        0, // hit group offset
        1, // geometry contribution multiplier
        0, // miss shader index
        ray,
        payload
    );
    
    // Read the result to exercise the read path
    float result = payload.hitValue;
    Output[0] = result;
}

[shader("raygeneration")]
void RayGenMain()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;
    
    // Primary ray
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;
    
    RayPayload payload;
    payload.depth = 0;
    
    // Trace primary ray - will invoke miss or closest hit
    TraceRay(
        AccelerationStructure,
        RAY_FLAG_NONE,
        0xFF,
        0, // hit group offset
        1, // geometry contribution multiplier
        0, // miss shader index
        ray,
        payload
    );
    
    // Read the result to exercise the read path
    float result = payload.hitValue;
    
    // Trace shadow ray if we hit something - will invoke miss shader at index 1
    if (result > 0)
    {
        ShadowPayload shadowPayload;
        
        RayDesc shadowRay;
        shadowRay.Origin = ray.Origin + ray.Direction * 0.5f;
        shadowRay.Direction = normalize(float3(1, 1, 0));
        shadowRay.TMin = 0.001f;
        shadowRay.TMax = 10000.0f;
        
        TraceRay(
            AccelerationStructure,
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF,
            0,
            1,
            1, // miss shader index 1 for shadows
            shadowRay,
            shadowPayload
        );
        // Read the result to exercise the read path
        result += shadowPayload.visibility;
    }
    
    // Invoke callable shader
    CallableData callableData;
    CallShader(0, callableData);
    result += callableData.result;
    
    Output[0] = result;
}

[shader("raygeneration")]
void RayGenMainAlt()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;
    
    // Alternative ray generation - different origin
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
    
    // Read result
    float result = payload.hitValue;
    
    // Invoke alternate callable shader
    CallableData callableData;
    CallShader(1, callableData);
    result += callableData.result;
    
    Output[0] = result;
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
    
    // Recursive ray if depth allows
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
        
        // Read to satisfy the qualifier
        float reflectionResult = reflectionPayload.hitValue;
    }
}

[shader("closesthit")]
void ClosestHitMainAlt(inout RayPayload payload, in HitAttributes attrib)
{
    payload.hitValue = 2;
    
    // Alternative behavior - different reflection
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
        
        // Read to satisfy the qualifier
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