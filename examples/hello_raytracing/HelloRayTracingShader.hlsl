#include <Vex.hlsli>

struct UniformStruct
{
    uint outputTextureHandle;
    uint accelerationStructureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const RWTexture2D<float4> OutputTexture = GetBindlessResource(Uniforms.outputTextureHandle);
static RaytracingAccelerationStructure AccelerationStructure = GetBindlessResource(Uniforms.accelerationStructureHandle);

// We use the built-in HLSL HitAttributes.
typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct [raypayload] RayPayload
{
    float3 color : write(closesthit, miss) : read(caller);
};

[shader("raygeneration")]
void RayGenMain()
{
    // Get the dispatch coordinates
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDimensions = DispatchRaysDimensions().xy;

    // HLSL writes to the left side of the screen.
    // if (launchIndex.x > launchDimensions.x / 2.0f)
    // {
    //     return;
    // }

    // Convert pixel coordinates to clip space (xy in [-1, 1])
    float2 clipPos = float2((float(launchIndex.x) + 0.5f) * 2.0f / float(launchDimensions.x) - 1.0f,
                            1.0f - (float(launchIndex.y) + 0.5f) * 2.0f / float(launchDimensions.y));

    RayDesc ray;
    ray.Direction = float3(0, 0, 1);
    ray.Origin = float3(clipPos, 0);
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating-point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001f;
    ray.TMax = 10000;

    RayPayload payload;
    TraceRay(
        AccelerationStructure, // Acceleration Structure
        RAY_FLAG_FORCE_OPAQUE, // Ray Flags
        0xFF,                  // Instance inclusion mask
        0,                     // Ray contribution to hitgroup index
        1,                     // Multiplier for geometry contribution to hit group index
        0,                     // MissShader index
        ray,                   // Ray Descriptor
        payload                // InOut RayPayload
    );

    // Write to output
    OutputTexture[launchIndex] = float4(payload.color, 1.0f);
}

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    // Sky color
    payload.color = float3(0.2, 0.3, 0.4);
}

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload, in HitAttributes attr)
{
    payload.color = InstanceID() == 0 ? float3(1, 1, 1) : float3(0, 1, 1);
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color *= float3(barycentrics);
}