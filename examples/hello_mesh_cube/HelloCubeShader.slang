import Vex;
#include "Common.hlsli"

static float3 offsets[2] = {
    float3(0.4f, -0.5f, -1),
    float3(0.8f, 0.5f, -1)
};

// Payload that will be shared to all of the groups of mesh shader instanced
groupshared Payload p;

[shader("amplification")]
[numthreads(1,1,1)]
void ASMain(uint instanceId : SV_GroupId)
{
    p.offset = offsets[instanceId];

    for (int i = 0; i < MESHLET_COUNT; i++)
    {
        p.meshletColor[i] = RandomColorFromIndex(i);
    }

    // 1 dispatch per face of the cube
    DispatchMesh(MESHLET_COUNT, 1, 1, p);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(PRIMITIVES_PER_MESHLET_COUNT, 1, 1)]
void MSMain(
    uint meshletId : SV_GroupID,
    uint facePrimitiveId : SV_GroupThreadId,
    uint globalPrimitiveId : SV_DispatchThreadID,
    in payload Payload payload,

    out vertices MSOutput vertices[VERTICES_PER_MESHLET_COUNT],
    out indices uint3 indices[PRIMITIVES_PER_MESHLET_COUNT],

    uniform uint vertexBufferHandle,
    uniform uint indexBufferHandle,
    uniform float time)
{
    // This group will output a total of 6 vertices and 2 triangles.
    // Each thread contributing to 3 vertices and 1 triangle.
    SetMeshOutputCounts(VERTICES_PER_MESHLET_COUNT, PRIMITIVES_PER_MESHLET_COUNT);

    let vertexBuffer = GetBindlessResource<StructuredBuffer<Vertex>>(vertexBufferHandle);
    let indexBuffer = GetBindlessResource<StructuredBuffer<uint>>(indexBufferHandle);

    vertices[facePrimitiveId * 3 + 0] = ProcessVertex(vertexBuffer[indexBuffer[globalPrimitiveId * 3 + 0]], -time, payload.offset, payload.meshletColor[meshletId]);
    vertices[facePrimitiveId * 3 + 1] = ProcessVertex(vertexBuffer[indexBuffer[globalPrimitiveId * 3 + 1]], -time, payload.offset, payload.meshletColor[meshletId]);
    vertices[facePrimitiveId * 3 + 2] = ProcessVertex(vertexBuffer[indexBuffer[globalPrimitiveId * 3 + 2]], -time, payload.offset, payload.meshletColor[meshletId]);

    indices[facePrimitiveId] = (3 * facePrimitiveId).xxx + uint3(0,1,2);
}

[shader("pixel")]
float4 PSMain(MSOutput input) : SV_Target
{
    return float4(input.color, 1);
}
