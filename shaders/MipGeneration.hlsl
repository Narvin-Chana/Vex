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
#if CONVERT_TO_SRGB
    return float4(ApplySRGBCurve(color.rgb), color.a);
#else
    return color;
#endif
}

// Specialization for float3 (RGB)
template<>
float3 PackColor<float3>(float3 color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(color);
#else
    return color;
#endif
}

// Specialization for float2 (RG)
template<>
float2 PackColor<float2>(float2 color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f)).xy;
#else
    return color;
#endif
}

// Specialization for float (R only)
template<>
float PackColor<float>(float color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f, 0.0f)).x;
#else
    return color;
#endif
}

TEXTURE_TYPE SampleTexture2D(Texture2D<TEXTURE_TYPE> texture, float2 uv, uint mip)
{
    return texture.SampleLevel(LinearSampler, uv, mip);
}

TEXTURE_TYPE SampleTexture2DArray(Texture2DArray<TEXTURE_TYPE> texture, float3 uvSlice, uint mip)
{
    return texture.SampleLevel(LinearSampler, uvSlice, mip);
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

TEXTURE_TYPE SampleTextureCube(TextureCube<TEXTURE_TYPE> texture, float2 uv, uint cubeFace, uint mip)
{
    float3 dir = CubeFaceUVToDirection(cubeFace, uv);
    return texture.SampleLevel(LinearSampler, dir, mip);
}

TEXTURE_TYPE SampleTextureCubeArray(TextureCubeArray<TEXTURE_TYPE> texture, float2 uv, uint cubeFace, uint slice, uint mip)
{
    float3 dir = CubeFaceUVToDirection(cubeFace, uv);
    return texture.SampleLevel(LinearSampler, float4(dir, slice), mip);
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
    TEXTURE_TYPE sample = SampleTexture2D(sourceMip, uv, MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    // > 2:1 ratio in x dimension.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTexture2D(sourceMip, uv, MipUniforms.sourceMipLevel) +
        SampleTexture2D(sourceMip, uv + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    // > 2:1 ratio in y dimension.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTexture2D(sourceMip, uv, MipUniforms.sourceMipLevel) +
        SampleTexture2D(sourceMip, uv + offset, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    // > 2:1 ratio in both dimensions.
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = SampleTexture2D(sourceMip, uv, MipUniforms.sourceMipLevel);
    sample += SampleTexture2D(sourceMip, uv + float2(offset.x, 0), MipUniforms.sourceMipLevel);
    sample += SampleTexture2D(sourceMip, uv + float2(0, offset.y), MipUniforms.sourceMipLevel);
    sample += SampleTexture2D(sourceMip, uv + float2(offset.x, offset.y), MipUniforms.sourceMipLevel);
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
    TEXTURE_TYPE sample = SampleTexture2DArray(sourceMip, float3(uv, dtid.z), MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTexture2DArray(sourceMip, float3(uv, dtid.z), MipUniforms.sourceMipLevel) +
        SampleTexture2DArray(sourceMip, float3(uv + offset, dtid.z), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTexture2DArray(sourceMip, float3(uv, dtid.z), MipUniforms.sourceMipLevel) +
        SampleTexture2DArray(sourceMip, float3(uv + offset, dtid.z), MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = SampleTexture2DArray(sourceMip, float3(uv, dtid.z), MipUniforms.sourceMipLevel);
    sample += SampleTexture2DArray(sourceMip, float3(uv + float2(offset.x, 0), dtid.z), MipUniforms.sourceMipLevel);
    sample += SampleTexture2DArray(sourceMip, float3(uv + float2(0, offset.y), dtid.z), MipUniforms.sourceMipLevel);
    sample += SampleTexture2DArray(sourceMip, float3(uv + float2(offset.x, offset.y), dtid.z), MipUniforms.sourceMipLevel);
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
void MipGenerationTextureCube(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    TextureCube<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
    // Extract face index from dtid.z
    uint faceIndex = dtid.z % 6;
    
#if NON_POWER_OF_TWO == 0
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + 0.5f.xx);
    TEXTURE_TYPE sample = SampleTextureCube(sourceMip, uv, faceIndex, MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTextureCube(sourceMip, uv, faceIndex, MipUniforms.sourceMipLevel) +
        SampleTextureCube(sourceMip, uv + offset, faceIndex, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTextureCube(sourceMip, uv, faceIndex, MipUniforms.sourceMipLevel) +
        SampleTextureCube(sourceMip, uv + offset, faceIndex, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = SampleTextureCube(sourceMip, uv, faceIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCube(sourceMip, uv + float2(offset.x, 0), faceIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCube(sourceMip, uv + float2(0, offset.y), faceIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCube(sourceMip, uv + float2(offset.x, offset.y), faceIndex, MipUniforms.sourceMipLevel);
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
void MipGenerationTextureCubeArray(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    TextureCubeArray<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    
    // Extract face index and cube array index from dtid.z
    uint faceIndex = dtid.z % 6;
    uint cubeArrayIndex = dtid.z / 6;
    
#if NON_POWER_OF_TWO == 0
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + 0.5f.xx);
    float3 dir = CubeFaceUVToDirection(faceIndex, uv);
    TEXTURE_TYPE sample = SampleTextureCubeArray(sourceMip, uv, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel);
#elif NON_POWER_OF_TWO == 1
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.5f));
    float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTextureCubeArray(sourceMip, uv, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel) +
        SampleTextureCubeArray(sourceMip, uv + offset, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 2
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.5f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * float2(0, 0.5f);
    float3 dir0 = CubeFaceUVToDirection(faceIndex, uv);
    float3 dir1 = CubeFaceUVToDirection(faceIndex, uv + offset);
    TEXTURE_TYPE sample = 0.5f * 
    (
        SampleTextureCubeArray(sourceMip, uv, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel) +
        SampleTextureCubeArray(sourceMip, uv + offset, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel)
    );
#elif NON_POWER_OF_TWO == 3
    float2 uv = MipUniforms.texelSize.xy * (dtid.xy + float2(0.25f, 0.25f));
    float2 offset = MipUniforms.texelSize.xy * 0.5f;
    TEXTURE_TYPE sample = SampleTextureCubeArray(sourceMip, uv, faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCubeArray(sourceMip, uv + float2(offset.x, 0), faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCubeArray(sourceMip, uv + float2(0, offset.y), faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel);
    sample += SampleTextureCubeArray(sourceMip, uv + float2(offset.x, offset.y), faceIndex, cubeArrayIndex, MipUniforms.sourceMipLevel);
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
