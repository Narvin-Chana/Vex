#include <Vex.hlsli>

#ifndef TEXTURE_TYPE
// Fallback to float4
#define TEXTURE_TYPE float4
#endif

#ifndef NON_POWER_OF_TWO
#define NON_POWER_OF_TWO 0
#endif

#ifndef LINEAR_SAMPLER_SLOT
#define LINEAR_SAMPLER_SLOT s0
#endif

SamplerState LinearSampler : register(LINEAR_SAMPLER_SLOT);

struct Uniforms
{
    float3 texelSize; // 1.0f / (mip0 texture size)
    uint sourceMipHandle;
    uint sourceMipLevel;
    uint numMips; // Number of dest mips, so 1 or 2.
    uint destinationMipHandle0;
    uint destinationMipHandle1;
};

VEX_UNIFORMS(Uniforms, MipUniforms);

float3 ApplySRGBCurve(float3 x)
{
    // This is exactly the sRGB curve (a bit more expensive).
    return select(x < 0.0031308f, 12.92f * x, 1.055f * pow(abs(x), 1.0f / 2.4f) - 0.055f);
}

// All non-float types go through here.
template<typename T>
T PackColor(T color)
{
    return color;
}

// Specialization for float4 (RGBA)
template<>
float4 PackColor<float4>(float4 color)
{
#ifdef CONVERT_TO_SRGB
    return float4(ApplySRGBCurve(color.rgb), color.a);
#else
    return color;
#endif
}

// Specialization for float3 (RGB)
template<>
float3 PackColor<float3>(float3 color)
{
#ifdef CONVERT_TO_SRGB
    return ApplySRGBCurve(color);
#else
    return color;
#endif
}

// Specialization for float2 (RG)
template<>
float2 PackColor<float2>(float2 color)
{
#ifdef CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f)).xy;
#else
    return color;
#endif
}

// Specialization for float (R only)
template<>
float PackColor<float>(float color)
{
#ifdef CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f, 0.0f)).x;
#else
    return color;
#endif
}

TEXTURE_TYPE DownsampleQuad(TEXTURE_TYPE sample)
{
    TEXTURE_TYPE sampleRight = QuadReadAcrossX(sample);
    TEXTURE_TYPE sampleDown = QuadReadAcrossY(sample);
    TEXTURE_TYPE sampleDiag = QuadReadAcrossDiagonal(sample);
    
    return 0.25f * (sample + sampleRight + sampleDown + sampleDiag);
}

[numthreads(8, 8, 1)]
void MipGenerationTexture2D(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    Texture2D<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
#if NON_POWER_OF_TWO == 0
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + 0.5f.xx);
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    // > 2:1 ratio in x dimension.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, uv + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    // > 2:1 ratio in y dimension.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, uv + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    // > 2:1 ratio in both dimensions.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uv + float2(offset.x, 0), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uv + float2(0, offset.y), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uv + float2(offset.x, offset.y), MipUniforms.sourceMipLevel);
    sample *= 0.25f;
#endif
    
    RWTexture2D<TEXTURE_TYPE> destinationMip0 = GetBindlessResource(MipUniforms.destinationMipHandle0);
    destinationMip0[dtid.xy] = PackColor<TEXTURE_TYPE>(sample);

    if (MipUniforms.numMips == 1)
    {
        return;
    }

    // This has to be called in every wave, otherwise the result of QuadReadAcross is undefined.
    TEXTURE_TYPE dstSample = DownsampleQuad(sample);
    
    // Only have 1 pixel out of 4 write to the second mip.
    const bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        RWTexture2D<TEXTURE_TYPE> destinationMip1 = GetBindlessResource(MipUniforms.destinationMipHandle1);
        destinationMip1[dtid.xy >> 1] = PackColor<TEXTURE_TYPE>(dstSample);
    }
}

[numthreads(8, 8, 1)]
void MipGenerationTexture2DArray(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    Texture2DArray<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
#if NON_POWER_OF_TWO == 0
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + 0.5f.xx);
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, float3(uv, dtid.z), MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, float3(uv, dtid.z), MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, float3(uv + offset, dtid.z), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, float3(uv, dtid.z), MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, float3(uv + offset, dtid.z), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, float3(uv, dtid.z), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float3(uv + float2(offset.x, 0), dtid.z), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float3(uv + float2(0, offset.y), dtid.z), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float3(uv + float2(offset.x, offset.y), dtid.z), MipUniforms.sourceMipLevel);
    sample *= 0.25f;
#endif
    
    RWTexture2DArray<TEXTURE_TYPE> destinationMip0 = GetBindlessResource(MipUniforms.destinationMipHandle0);
    destinationMip0[dtid.xyz] = PackColor<TEXTURE_TYPE>(sample);
    
    if (MipUniforms.numMips == 1)
    {
        return;
    }
    
    TEXTURE_TYPE dstSample = DownsampleQuad(sample);
    
    const bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        RWTexture2DArray<TEXTURE_TYPE> destinationMip1 = GetBindlessResource(MipUniforms.destinationMipHandle1);
        destinationMip1[uint3(dtid.xy >> 1, dtid.z)] = PackColor<TEXTURE_TYPE>(dstSample);
    }
}

// TextureCube requires a direction, we compute UVs. So conversion must be done.
float3 CubeFaceUVToDirection(uint face, float2 uv)
{
    // Convert UV from [0,1] to [-1,1]
    float2 coords = uv * 2.0f - 1.0f;
    
    // Face mapping (HLSL convention):
    // 0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z
    switch (face)
    {
        case 0:
            return float3(1.0f, -coords.y, -coords.x); // +X
        case 1:
            return float3(-1.0f, -coords.y, coords.x); // -X
        case 2:
            return float3(coords.x, 1.0f, coords.y); // +Y
        case 3:
            return float3(coords.x, -1.0f, -coords.y); // -Y
        case 4:
            return float3(coords.x, -coords.y, 1.0f); // +Z
        case 5:
            return float3(-coords.x, -coords.y, -1.0f); // -Z
        default:
            return float3(0, 0, 1);
    }
}

[numthreads(8, 8, 1)]
void MipGenerationTextureCube(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    TextureCubeArray<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
    // Extract face index and cube array index from dtid.z
    uint faceIndex = dtid.z % 6;
    uint cubeArrayIndex = dtid.z / 6;
    
#if NON_POWER_OF_TWO == 0
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + 0.5f.xx);
    float3 dir = CubeFaceUVToDirection(faceIndex, uv);
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, float4(dir, cubeArrayIndex), MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    float3 dir0 = CubeFaceUVToDirection(faceIndex, uv);
    float3 dir1 = CubeFaceUVToDirection(faceIndex, uv + offset);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, float4(dir0, cubeArrayIndex), MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, float4(dir1, cubeArrayIndex), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    float3 dir0 = CubeFaceUVToDirection(faceIndex, uv);
    float3 dir1 = CubeFaceUVToDirection(faceIndex, uv + offset);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, float4(dir0, cubeArrayIndex), MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, float4(dir1, cubeArrayIndex), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    float3 dir0 = CubeFaceUVToDirection(faceIndex, uv);
    float3 dir1 = CubeFaceUVToDirection(faceIndex, uv + float2(offset.x, 0));
    float3 dir2 = CubeFaceUVToDirection(faceIndex, uv + float2(0, offset.y));
    float3 dir3 = CubeFaceUVToDirection(faceIndex, uv + float2(offset.x, offset.y));
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, float4(dir0, cubeArrayIndex), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float4(dir1, cubeArrayIndex), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float4(dir2, cubeArrayIndex), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, float4(dir3, cubeArrayIndex), MipUniforms.sourceMipLevel);
    sample *= 0.25f;
#endif
    
    RWTexture2DArray<TEXTURE_TYPE> destinationMip0 = GetBindlessResource(MipUniforms.destinationMipHandle0);
    destinationMip0[dtid.xyz] = PackColor<TEXTURE_TYPE>(sample);
    
    if (MipUniforms.numMips == 1)
    {
        return;
    }
    
    TEXTURE_TYPE dstSample = DownsampleQuad(sample);
    
    const bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        RWTexture2DArray<TEXTURE_TYPE> destinationMip1 = GetBindlessResource(MipUniforms.destinationMipHandle1);
        destinationMip1[uint3(dtid.xy >> 1, dtid.z)] = PackColor<TEXTURE_TYPE>(dstSample);
    }
}

[numthreads(8, 8, 1)]
void MipGenerationTexture3D(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    Texture3D<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
#if NON_POWER_OF_TWO == 0
    float3 uvw = MipUniforms.texelSize.xyz * (dtid.xyz + 0.5f.xxx);
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float3 uvw = MipUniforms.texelSize.xyz * (dtid.xyz + float3(0.25f, 0.5f, 0.5f));
    float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, uvw + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float3 uvw = MipUniforms.texelSize.xyz * (dtid.xyz + float3(0.5f, 0.25f, 0.5f));
    float3 offset = MipUniforms.texelSize.xyz * float3(0, 0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        sourceMip.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel) +
        sourceMip.SampleLevel(LinearSampler, uvw + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float3 uvw = MipUniforms.texelSize.xyz * (dtid.xyz + float3(0.25f, 0.25f, 0.5f));
    float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0.5f, 0);
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uvw + float3(offset.x, 0, 0), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uvw + float3(0, offset.y, 0), MipUniforms.sourceMipLevel);
    sample += sourceMip.SampleLevel(LinearSampler, uvw + float3(offset.x, offset.y, 0), MipUniforms.sourceMipLevel);
    sample *= 0.25f;
#endif
    
    // For Texture3D we would technically have to support all permutations of odd/even dimensions, but this would lead to permutation explosion.
    // Since the end result of this is just a bit of lost precision, which we accept anyways when generating mips, we can accept this.
    
    RWTexture3D<TEXTURE_TYPE> destinationMip0 = GetBindlessResource(MipUniforms.destinationMipHandle0);
    destinationMip0[dtid.xyz] = PackColor<TEXTURE_TYPE>(sample);
    
    if (MipUniforms.numMips == 1)
    {
        return;
    }
    
    TEXTURE_TYPE dstSample = DownsampleQuad(sample);
    
    const bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        RWTexture3D<TEXTURE_TYPE> destinationMip1 = GetBindlessResource(MipUniforms.destinationMipHandle1);
        destinationMip1[dtid.xyz >> 1] = PackColor<TEXTURE_TYPE>(dstSample);
    }
}