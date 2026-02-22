import Vex;

struct HitAttributes
{
    float2 dummy;
};

struct [raypayload] RayPayload
{
    float res : write(closesthit, miss, caller) : read(caller);
};

[shader("raygeneration")]
void RayGenMain(
    uniform uint outputHandle,
    uniform uint accelerationStructureHandle)
{
    let Output = GetBindlessResource<RWStructuredBuffer<float>>(outputHandle);
    let AccelerationStructure = GetBindlessResource<RaytracingAccelerationStructure>(accelerationStructureHandle);

    RayDesc ray;
    ray.Direction = float3(0, 0, 1);
    ray.Origin    = float3(0.5f, 0.5f, 0);
    ray.TMin      = 0.001f;
    ray.TMax      = 10000;

    RayPayload payload;
    payload.res = -10;

    TraceRay(
        AccelerationStructure, // Acceleration Structure
        RAY_FLAG_FORCE_OPAQUE, // Ray Flags
        0xFF,                  // Instance inclusion mask
        0,                     // Ray contribution to hitgroup index
        1,                     // Multiplier for geometry contribution to hit group index
        0,                     // Miss shader index
        ray,                   // Ray Descriptor
        payload                // InOut RayPayload
    );

    Output[0] = payload.res;
}

[shader("miss")]
void MissMain(
    uniform uint outputHandle,
    uniform uint accelerationStructureHandle,
    inout RayPayload payload)
{
    payload.res = -1;
}

[shader("intersection")]
void IntersectMain(
    uniform uint outputHandle,
    uniform uint accelerationStructureHandle)
{
    float tHit = 0.8f;
    HitAttributes attr = (HitAttributes) 0;
    ReportHit(tHit, 0, attr);
}

[shader("closesthit")]
void ClosestHitMain(
    uniform uint outputHandle,
    uniform uint accelerationStructureHandle,
    inout RayPayload payload,
    in HitAttributes attr)
{
    payload.res = 1;
}
