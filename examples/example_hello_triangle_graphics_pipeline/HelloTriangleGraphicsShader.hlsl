void ComputeTriangleVertex(in uint vertexID, out float4 position, out float2 uv)
{
    uv = float2((vertexID << 1) & 2, vertexID & 2);
    position = float4(uv.x * 2 - 1, -uv.y * 2 + 1, 0, 1);
}

void VSMain(uint vertexID : SV_VertexID, out float4 pos : SV_POSITION)
{
    float2 uv;
    ComputeTriangleVertex(3 - vertexID, pos, uv);
}

void PSMain(float4 position : SV_POSITION, out float4 outColor : SV_Target)
{
    outColor = float4(position.xyz, 1);
}