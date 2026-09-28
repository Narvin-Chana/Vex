#include <Vex.hlsli>
#include "Common.hlsli"

struct UniformStruct
{
    uint vertexBufferHandle;
    float time;
    uint uvGuideTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

struct Vertex
{
    float3 position;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(in uint vertexID : SV_VertexID)
{
    StructuredBuffer<Vertex> vertexBuffer = GetBindlessResource(Uniforms.vertexBufferHandle);
    Vertex vertex = vertexBuffer[vertexID];

    VSOutput vs;
    vs.uv = vertex.uv;
    vs.position = ProjectVertex(TransformVertex(vertex.position, Uniforms.time) + float3(-0.4f, 0.3f, -1));
    return vs;
}

static const Texture2D<float4> UVGuideTexture = GetBindlessResource(Uniforms.uvGuideTextureHandle);

VEX_STATIC_SAMPLER(LinearSampler, 0);

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(UVGuideTexture.Sample(LinearSampler, input.uv).rgb, 1);
}
