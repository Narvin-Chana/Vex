#include <Vex.hlsli>

#define TEXTURE_DIMENSION_2D 0
#define TEXTURE_DIMENSION_2DARRAY 1
#define TEXTURE_DIMENSION_CUBE 2
#define TEXTURE_DIMENSION_CUBEARRAY 3
#define TEXTURE_DIMENSION_3D 4

#ifndef TEXTURE_DIM
#error Must define a texture dimension.
#endif

struct Uniforms
{
    uint linearSamplerHandle;
    float3 texelSize; // 1.0f / (source mip dimensions)
    uint sourceMipHandle;
    uint sourceMipLevel;
    uint numMips; // Number of dest mips: 1 or 2
    uint destinationMipHandle0;
    uint destinationMipHandle1;
    uint nonPowerOfTwo;
    uint convertToSRGB;
    uint numChannels;
};

VEX_UNIFORMS(Uniforms, MipUniforms);

// ============================================================================
// SRGB Conversion
// ============================================================================

float3 ApplySRGBCurve(float3 x)
{
    return select(x < 0.0031308f, 12.92f * x, 1.055f * pow(abs(x), 1.0f / 2.4f) - 0.055f);
}

float4 PackColorChannels(float4 color, uint numChannels)
{
    if (!MipUniforms.convertToSRGB)
        return color;
    if (numChannels == 1)
        return float4(ApplySRGBCurve(float3(color.r, 0, 0)), 1);
    if (numChannels == 2)
        return float4(ApplySRGBCurve(float3(color.rg, 0)), 1);
    if (numChannels == 3)
        return float4(ApplySRGBCurve(color.rgb), 1);
    if (numChannels == 4)
        return float4(ApplySRGBCurve(color.rgb), color.a);
    return color;
}

float3 CubeFaceUVToDirection(uint face, float2 uv)
{
    float2 coords = uv * 2.0f - 1.0f;

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

float4 SampleAt(uint3 coord, float2 uv)
{
    SamplerState LinearSampler = GetBindlessSampler(0);
#if TEXTURE_DIM == TEXTURE_DIMENSION_2D
    {
        Texture2D<float4> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
        return tex.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel);
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_2DARRAY
    {
        Texture2DArray<float4> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
        return tex.SampleLevel(LinearSampler, float3(uv, coord.z), MipUniforms.sourceMipLevel);
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_CUBE
    {
        TextureCube<float4> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
        uint faceIndex = coord.z % 6;
        float3 dir = CubeFaceUVToDirection(faceIndex, uv);
        return tex.SampleLevel(LinearSampler, dir, MipUniforms.sourceMipLevel);
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_CUBEARRAY
    {
        TextureCubeArray<float4> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
        uint faceIndex = coord.z % 6;
        uint cubeIndex = coord.z / 6;
        float3 dir = CubeFaceUVToDirection(faceIndex, uv);
        return tex.SampleLevel(LinearSampler, float4(dir, cubeIndex), MipUniforms.sourceMipLevel);
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_3D
    {
        Texture3D<float4> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
        float3 uvw = float3(uv, MipUniforms.texelSize.z * (coord.z + 0.5f));
        return tex.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel);
    }
#endif

    return float4(1, 0, 0, 1);
}

float4 SampleWithNPOT(uint3 coord)
{
#if TEXTURE_DIM == TEXTURE_DIMENSION_3D
    // Texture3D needs special handling
    {
        if (MipUniforms.nonPowerOfTwo == 0)
        {
            // Power-of-two in all dimensions
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + 0.5f);
            return SampleAt(coord, uvw.xy); // SampleAt needs to handle uvw for 3D
        }
        else if(MipUniforms.nonPowerOfTwo == 1)
        {
            // > 2:1 ratio in X only
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.5f, 0.5f));
            float3 offsetX = MipUniforms.texelSize.xyz * float3(0.5f, 0.0f, 0.0f);
            return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetX).xy));
        }
        else if (MipUniforms.nonPowerOfTwo == 2)
        {
            // > 2:1 ratio in Y only
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.25f, 0.5f));
            float3 offsetY = MipUniforms.texelSize.xyz * float3(0.0f, 0.5f, 0.0f);
            return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetY).xy));
        }
        else if (MipUniforms.nonPowerOfTwo == 3)
        {
            // > 2:1 ratio in X and Y
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.25f, 0.5f));
            float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0.5f, 0.0f);
            float4 s = SampleAt(coord, uvw.xy);
            s += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(offset.x, offset.y, 0.0f)).xy);
            return s * 0.25f;
        }
        else if (MipUniforms.nonPowerOfTwo == 4)
        {
            // > 2:1 ratio in Z only
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.5f, 0.25f));
            float3 offsetZ = MipUniforms.texelSize.xyz * float3(0.0f, 0.0f, 0.5f);
            return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetZ).xy));
        }
        else if (MipUniforms.nonPowerOfTwo == 5)
        {
            // > 2:1 ratio in X and Z
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.5f, 0.25f));
            float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0.0f, 0.5f);
            float4 s = SampleAt(coord, uvw.xy);
            s += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
            s += SampleAt(coord, (uvw + float3(offset.x, 0.0f, offset.z)).xy);
            return s * 0.25f;
        }
        else if (MipUniforms.nonPowerOfTwo == 6)
        {
            // > 2:1 ratio in Y and Z
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.25f, 0.25f));
            float3 offset = MipUniforms.texelSize.xyz * float3(0.0f, 0.5f, 0.5f);
            float4 s = SampleAt(coord, uvw.xy);
            s += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, offset.y, offset.z)).xy);
            return s * 0.25f;
        }
        else if (MipUniforms.nonPowerOfTwo == 7)
        {
            // > 2:1 ratio in all dimensions (X, Y, Z)
            float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + 0.25f);
            float3 offset = MipUniforms.texelSize.xyz * 0.5f;
            float4 s = SampleAt(coord, uvw.xy);
            s += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(offset.x, offset.y, 0.0f)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
            s += SampleAt(coord, (uvw + float3(offset.x, 0.0f, offset.z)).xy);
            s += SampleAt(coord, (uvw + float3(0.0f, offset.y, offset.z)).xy);
            s += SampleAt(coord, (uvw + float3(offset.x, offset.y, offset.z)).xy);
            return s * 0.125f;
        }
    }
#else
    {
        // 2D, 2DArray, Cube, CubeArray
        if (MipUniforms.nonPowerOfTwo == 0)
        {
            float2 uv = MipUniforms.texelSize.xy * (coord.xy + 0.5f);
            return SampleAt(coord, uv);
        }
        else if (MipUniforms.nonPowerOfTwo == 1)
        {
            float2 uv = MipUniforms.texelSize.xy * (coord.xy + float2(0.25f, 0.5f));
            float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0.0f);
            return 0.5f * (SampleAt(coord, uv) + SampleAt(coord, uv + offset));
        }
        else if (MipUniforms.nonPowerOfTwo == 2)
        {
            float2 uv = MipUniforms.texelSize.xy * (coord.xy + float2(0.5f, 0.25f));
            float2 offset = MipUniforms.texelSize.xy * float2(0.0f, 0.5f);
            return 0.5f * (SampleAt(coord, uv) + SampleAt(coord, uv + offset));
        }
        else if(MipUniforms.nonPowerOfTwo == 3)
        {
            float2 uv = MipUniforms.texelSize.xy * (coord.xy + 0.25f);
            float2 offset = MipUniforms.texelSize.xy * 0.5f;
            float4 s = SampleAt(coord, uv);
            s += SampleAt(coord, uv + float2(offset.x, 0.0f));
            s += SampleAt(coord, uv + float2(0.0f, offset.y));
            s += SampleAt(coord, uv + offset);
            return s * 0.25f;
        }
    }
#endif

    return float4(1, 0, 0, 1);
}

void WriteMip0(uint3 coord, float4 color)
{
#if TEXTURE_DIM == TEXTURE_DIMENSION_2D
    {
        RWTexture2D<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
        dst[coord.xy] = color;
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_3D
    // All array/cube/3D types use Texture2DArray or RWTexture3D
    {
        RWTexture3D<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
        dst[coord.xyz] = color;
    }
#else
    {
        RWTexture2DArray<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
        dst[coord.xyz] = color;
    }
#endif
}

void WriteMip1(uint3 coord, float4 color)
{
#if TEXTURE_DIM == TEXTURE_DIMENSION_2D
    {
        RWTexture2D<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
        dst[coord.xy >> 1] = color;
    }
#elif TEXTURE_DIM == TEXTURE_DIMENSION_3D
    {
        RWTexture3D<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
        dst[coord.xyz >> 1] = color;
    }
#else
    {
        RWTexture2DArray<float4> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
        dst[uint3(coord.xy >> 1, coord.z)] = color;
    }
#endif
}

[numthreads(8, 8, 4)]
void MipGenerationCS(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint3 coord = uint3(dtid.xy, gid.z);

    // Since we no longer branch numthreads() at compile-time, gtid must be handled after dispatch, which can be less performant.
#if TEXTURE_DIM == TEXTURE_DIMENSION_3D
    {
        if (gtid.x >= 4 || gtid.y >= 4)
        {
            return;
        }
        coord = uint3(gtid.xy, dtid.z);
    }
#else
    if (gtid.z >= 1)
    {
        return;
    }
#endif

    // Sample and downsample first mip
    float4 sampleCenter = SampleWithNPOT(coord);

    // Write first output mip
    WriteMip0(coord, PackColorChannels(sampleCenter, MipUniforms.numChannels));

    // Early exit if we're only generating one mip
    if (MipUniforms.numMips == 1)
    {
        return;
    }

    // Generate second mip using wave intrinsics
    // ddx() is temporary, it forces the SPV_KHR_compute_shader_derivatives extension to be emitted by DXC, allowing for threads to be grouped in 2x2 quads.
    // An issue was opened and fixed in the DXC repo: https://github.com/microsoft/DirectXShaderCompiler/issues/7943
    // We are waiting for it to be integrated in the latest DXC release.
    float4 sampleRight = float4(ddx(sampleCenter) * 0.00001f + QuadReadAcrossX(sampleCenter));
    float4 sampleDown = QuadReadAcrossY(sampleCenter);
    float4 sampleDiag = QuadReadAcrossDiagonal(sampleCenter);
    float4 dstSample = float4(0.25f * (sampleCenter + sampleRight + sampleDown + sampleDiag));

    // Only one thread per 2x2 quad writes to second mip
    bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        WriteMip1(coord, PackColorChannels(dstSample, MipUniforms.numChannels));
    }
}
