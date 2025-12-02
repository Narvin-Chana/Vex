[shader("vertex")]
float4 ReflectionVertex1(in float3 pos : POSITION, in float2 uv : TEXCOORD) : SV_Position
{
    return float4(pos.xxx, uv.x);
}

[shader("vertex")]
float4 ReflectionVertex2(in float3 pos : POSITION, in float2 uv : TEXCOORD, in uint vertexId : SV_VertexID, in uint instanceId : SV_InstanceID) : SV_Position
{
    return float4(pos.xx, uv.x, float(vertexId + instanceId));
}

[shader("vertex")]
float4 ReflectionVertex3(in float3 pos0 : POSITION0, in float3 pos1 : POSITION1, in float2 uv : TEXCOORD1) : SV_Position
{
    return float4(pos0.xx, pos1.x, uv.x);
}

// PIXEL SHADER

[shader("pixel")]
float4 ReflectionPixel1(in float3 barycentrics : SV_Barycentrics) : SV_Target
{
    return float4(barycentrics, 1);
}

// COMPUTE SHADER

[shader("compute")]
[numthreads(1, 1, 1)]
void ReflectionCompute1(uint groupId : SV_GroupID)
{
}

[shader("compute")]
[numthreads(1, 1, 1)]
void ReflectionCompute2(uint threadID : SV_GroupThreadID)
{
}

[shader("compute")]
[numthreads(1, 1, 1)]
void ReflectionCompute3(uint groupId : SV_GroupID, uint threadID : SV_GroupThreadID)
{
}