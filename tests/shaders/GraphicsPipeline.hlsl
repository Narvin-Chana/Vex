
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    return float4(uv.x * 2 - 1, -uv.y * 2 + 1, 0, 1);
}