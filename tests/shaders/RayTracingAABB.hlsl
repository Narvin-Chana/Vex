#include <Vex.hlsli>

struct UniformStruct
{
    uint outputHandle;
    uint accelerationStructureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const RWStructuredBuffer<float> Output = GetBindlessResource(Uniforms.outputHandle);
static RaytracingAccelerationStructure AccelerationStructure = GetBindlessResource(Uniforms.accelerationStructureHandle);

struct HitAttributes
{
    float2 dummy;
};

struct [raypayload] RayPayload
{
    float res : write(closesthit, miss, caller) : read(caller);
};

[shader("raygeneration")]
void RayGenMain()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;
    
    RayDesc ray;
    ray.Direction = float3(0, 0, 1);
    ray.Origin = float3(0.5f, 0.5f, 0);
    ray.TMin = 0.001f;
    ray.TMax = 10000;

    RayPayload payload;
    payload.res = -10;
    
    TraceRay(
        AccelerationStructure, // Acceleration Structure
        RAY_FLAG_FORCE_OPAQUE, // Ray Flags, see this for more info: https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag
        0xFF, // Instance inclusion mask
        0, // Ray contribution to hitgroup index
        1, // Multiplier for geometry contribution to hit group index
        0, // MissShader index
        ray, // Ray Descriptor
        payload // InOut RayPayload
    );

    Output[0] = payload.res;
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.res = 2;
}

[shader("intersection")]
void IntersectMain()
{
    // Simple test: always report a hit at a fixed distance
    // If this runs, it means the AABB system is working
    float tHit = 1;
    
    HitAttributes attr;
    attr.dummy = 0;
    
    // Report the hit - this will trigger the closest hit shader
    ReportHit(tHit, 0, attr);
}

[shader("closesthit")]
void ClosestHitMain(inout RayPayload payload, in HitAttributes attr)
{
    payload.res = 1;
}
