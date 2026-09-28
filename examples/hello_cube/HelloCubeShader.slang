import Vex;
#include "Common.hlsli"

struct Vertex
{
    float3 position;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(in uint vertexID : SV_VertexID, uniform uint vertexBufferHandle, uniform float time)
{
    let vertexBuffer = GetBindlessResource<StructuredBuffer<Vertex>>(vertexBufferHandle);
    Vertex vertex = vertexBuffer[vertexID];

    VSOutput vs;
    vs.uv = vertex.uv;
    vs.position = ProjectVertex(TransformVertex(vertex.position, -time) + float3(0.4f, -0.3f, -1));
    return vs;
}

// Vex stores samplers in set/space 1.
[vk::binding(0, 1)] SamplerState LinearSampler : register(s0, space1);

[shader("pixel")]
float4 PSMain(VSOutput input, uniform uint unusedVertexBufferHandle, uniform float time, uniform uint uvGuideTextureHandle)
    : SV_Target
{
    let UVGuideTexture = GetBindlessResource<Texture2D<float4>>(uvGuideTextureHandle);
    return float4(UVGuideTexture.Sample(LinearSampler, input.uv).rgb, 1);
}
