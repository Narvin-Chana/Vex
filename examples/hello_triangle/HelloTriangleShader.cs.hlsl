struct Colors
{
    float4 cols;
};

VEX_SHADER
{
	VEX_GLOBAL_RESOURCE(RWTexture2D<float4>, OutputTexture);
	VEX_GLOBAL_RESOURCE(RWStructuredBuffer<float4>, CommBuffer);
	VEX_GLOBAL_RESOURCE(ConstantBuffer<Colors>, ColorBuffer);
}

// Sourced from IQuilez SDF functions
float dot2(float2 v)
{
    return dot(v, v);
}

float sdf(float2 p)
{
    p.x = abs(p.x);

    if (p.y + p.x > 1.0)
        return sqrt(dot2(p - float2(0.25, 0.75))) - sqrt(2.0f) / 4.0f;
    return sqrt(min(dot2(p - float2(0.00, 1.00)), dot2(p - 0.5f * max(p.x + p.y, 0.0f)))) * sign(p.x - p.y);
}

[numthreads(8, 8, 1)] void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);

    // Convert pixel coordinates to normalized space for opengl-based sdf function (-1 to 1)
    float2 uv = float2(dtid.xy) / max(width, height).xx * 2 - 1;

    float4 color = ColorBuffer.cols;
    CommBuffer[0] = float4(1, 1, 1, 1) - color;

    uv.y += 0.25f;
    uv.y *= -1;
    uv /= 0.63f;

    if (sdf(uv) < 0)
    {
        OutputTexture[dtid.xy] = float4(color.rgb, 1.0f);
    }
    else
    {
        OutputTexture[dtid.xy] = float4(0.2f, 0.2f, 0.2f, 1.0f);
    }
}
