#include <Vex.hlsli>

struct PassConstants
{
    uint hdrTextureHandle;
};
VEX_UNIFORMS(PassConstants, PassData);

void ComputeTriangleVertex(in uint vertexID, out float4 position, out float2 uv)
{
    uv = float2((vertexID << 1) & 2, vertexID & 2);
    position = float4(uv.x * 2 - 1, -uv.y * 2 + 1, 0, 1);
}

SamplerState LinearSampler;

void FullscreenTriangleVS(in uint vertexID : SV_VERTEXID, out float4 outPosition : SV_POSITION, out float2 outUV : TEXCOORD)
{
    ComputeTriangleVertex(2 - vertexID, outPosition, outUV);
}

float3 Rec709ToRec2020(float3 rec709Linear)
{
    // This is a chromatic adaptation matrix
    // It converts between the different RGB primaries
    static const float3x3 conversionMatrix = float3x3(
        0.627404, 0.329283, 0.043313,
        0.069097, 0.919541, 0.011362,
        0.016391, 0.088013, 0.895595
    );
    
    return mul(conversionMatrix, rec709Linear);
}

float3 ApplyST2084Curve(float3 linearColor)
{
    // Constants are sourced from wikipedia definition of the Perceptual Quantizer transfer function.
    static const float m1 = 2610.0f / 16384.0f;
    static const float m2 = 128.0f * 2523.0f / 4096.0f;
    static const float c1 = 3424.0f / 4096.0f;
    static const float c2 = 32.0f * 2413.0f / 4096.0f;
    static const float c3 = 32.0f * 2392.0f / 4096.0f;
    
    // Normalize: 1.0 = 10,000 nits
    float3 Y = linearColor / 10000.0;
    Y = pow(max(Y, 0.0), m1);
    
    float3 num = c1 + c2 * Y;
    float3 den = 1.0 + c3 * Y;
    float3 pq = pow(num / den, m2);
    
    return pq;
}

#define COLOR_SPACE_NONE  0
#define COLOR_SPACE_sRGB  1
#define COLOR_SPACE_scRGB 2
#define COLOR_SPACE_HDR10 3

#ifndef COLOR_SPACE
#define COLOR_SPACE COLOR_SPACE_NONE
#endif

static const float HDRExposure = 200.0f;

float4 TonemapPS(in float4 position : SV_POSITION, in float2 uv : TEXCOORD) : SV_Target
{
    Texture2D<float3> hdrTexture = GetBindlessResource(PassData.hdrTextureHandle);

    float3 color = hdrTexture.Sample(LinearSampler, uv);
    
    if (COLOR_SPACE >= COLOR_SPACE_scRGB)
    {
        // Apply HDR exposure (chosen empirically).
        color *= HDRExposure;
    }
    
#if COLOR_SPACE == COLOR_SPACE_NONE
    // COLOR_SPACE_NONE means we let the output incorrectly be sent to the swapchan as-is.
#elif COLOR_SPACE == COLOR_SPACE_sRGB
    // Simple reinhard tonemap.
    color = color / (1.0f + color);
#elif COLOR_SPACE == COLOR_SPACE_scRGB
    color = color / 80.0f;
#elif COLOR_SPACE == COLOR_SPACE_HDR10
    color = Rec709ToRec2020(color);
    color = ApplyST2084Curve(color);
#endif
    
    return float4(color, 1);
}
