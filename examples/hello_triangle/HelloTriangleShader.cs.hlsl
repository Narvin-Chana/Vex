#include <Vex.hlsli>

struct Colors
{
    float4 cols;
};

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

struct UniformStruct
{
    uint colorBufferHandle;
    uint commBufferHandle;
    uint outputTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

// HLSL way of declaring global resources:
static const RWTexture2D<float4> OutputTexture = GetBindlessResource(Uniforms.outputTextureHandle);
static const ConstantBuffer<Colors> ColorBuffer = GetBindlessResource(Uniforms.colorBufferHandle);
static const RWStructuredBuffer<float4> CommBuffer = GetBindlessResource(Uniforms.commBufferHandle);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);

    // HLSL writes to the left side of the screen.
    if (dtid.x > width / 2.0f)
    {
        return;
    }

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
        float2 globalUV = float2(dtid.xy) / max(width, height);
        OutputTexture[dtid.xy] = float4(0.2f, 0.2f, 0.2f, 1.0f);
    }
}
