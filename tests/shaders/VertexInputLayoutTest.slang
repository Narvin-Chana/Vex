struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL0;
    float3 tangent : NORMAL1;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput vs;
    vs.position = float4(input.position, input.uv.x);
    return vs;
}

[shader("pixel")]
float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.position.rgb, 1);
}
