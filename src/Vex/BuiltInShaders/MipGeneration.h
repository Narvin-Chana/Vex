#pragma once

#include <string_view>

#include <Vex/Shaders/ShaderKey.h>

namespace vex
{

static constexpr std::string_view MipGenerationEntryPoint = "MipGenerationCS";

// Candidate for #embed in C++26
static constexpr std::string_view MipGenerationSource = R"SHADER(

#include <Vex.hlsli>

#ifndef TEXTURE_TYPE
#define TEXTURE_TYPE float4
#endif

#ifndef NON_POWER_OF_TWO
#define NON_POWER_OF_TWO 0
#endif

#define TEXTURE_DIMENSION_2D 0
#define TEXTURE_DIMENSION_2DARRAY 1
#define TEXTURE_DIMENSION_CUBE 2
#define TEXTURE_DIMENSION_CUBEARRAY 3
#define TEXTURE_DIMENSION_3D 4

#ifndef TEXTURE_DIMENSION
#define TEXTURE_DIMENSION TEXTURE_DIMENSION_2D
#endif

#ifndef LINEAR_SAMPLER_SLOT
#define LINEAR_SAMPLER_SLOT s0
#endif

SamplerState LinearSampler : register(LINEAR_SAMPLER_SLOT);

struct Uniforms
{
    float3 texelSize; // 1.0f / (source mip dimensions)
    uint sourceMipHandle;
    uint sourceMipLevel;
    uint numMips; // Number of dest mips: 1 or 2
    uint destinationMipHandle0;
    uint destinationMipHandle1;
};

VEX_UNIFORMS(Uniforms, MipUniforms);

// ============================================================================
// SRGB Conversion
// ============================================================================

float3 ApplySRGBCurve(float3 x)
{
    return select(x < 0.0031308f, 12.92f * x, 1.055f * pow(abs(x), 1.0f / 2.4f) - 0.055f);
}

template<typename T>
T PackColor(T color)
{
    return color;
}

template<>
float4 PackColor<float4>(float4 color)
{
#if CONVERT_TO_SRGB
    return float4(ApplySRGBCurve(color.rgb), color.a);
#else
    return color;
#endif
}

template<>
float3 PackColor<float3>(float3 color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(color);
#else
    return color;
#endif
}

template<>
float2 PackColor<float2>(float2 color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f)).xy;
#else
    return color;
#endif
}

template<>
float PackColor<float>(float color)
{
#if CONVERT_TO_SRGB
    return ApplySRGBCurve(float3(color, 0.0f, 0.0f)).x;
#else
    return color;
#endif
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

TEXTURE_TYPE SampleAt(uint3 coord, float2 uv)
{
#if TEXTURE_DIMENSION == 0 // Texture2D
    Texture2D<TEXTURE_TYPE> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
    return tex.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel);
    
#elif TEXTURE_DIMENSION == 1 // Texture2DArray
    Texture2DArray<TEXTURE_TYPE> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
    return tex.SampleLevel(LinearSampler, float3(uv, coord.z), MipUniforms.sourceMipLevel);
    
#elif TEXTURE_DIMENSION == 2 // TextureCube
    TextureCube<TEXTURE_TYPE> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
    uint faceIndex = coord.z % 6;
    float3 dir = CubeFaceUVToDirection(faceIndex, uv);
    return tex.SampleLevel(LinearSampler, dir, MipUniforms.sourceMipLevel);
    
#elif TEXTURE_DIMENSION == 3 // TextureCubeArray
    TextureCubeArray<TEXTURE_TYPE> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
    uint faceIndex = coord.z % 6;
    uint cubeIndex = coord.z / 6;
    float3 dir = CubeFaceUVToDirection(faceIndex, uv);
    return tex.SampleLevel(LinearSampler, float4(dir, cubeIndex), MipUniforms.sourceMipLevel);
    
#elif TEXTURE_DIMENSION == 4 // Texture3D
    Texture3D<TEXTURE_TYPE> tex = GetBindlessResource(MipUniforms.sourceMipHandle);
    float3 uvw = float3(uv, MipUniforms.texelSize.z * (coord.z + 0.5f));
    return tex.SampleLevel(LinearSampler, uvw, MipUniforms.sourceMipLevel);
#endif
}

TEXTURE_TYPE SampleWithNPOT(uint3 coord)
{
#if TEXTURE_DIMENSION == TEXTURE_DIMENSION_3D // Texture3D needs special handling
    #if NON_POWER_OF_TWO == 0
        // Power-of-two in all dimensions
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + 0.5f);
        return SampleAt(coord, uvw.xy); // SampleAt needs to handle uvw for 3D
    #elif NON_POWER_OF_TWO == 1
        // > 2:1 ratio in X only
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.5f, 0.5f));
        float3 offsetX = MipUniforms.texelSize.xyz * float3(0.5f, 0.0f, 0.0f);
        return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetX).xy));
    #elif NON_POWER_OF_TWO == 2
        // > 2:1 ratio in Y only
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.25f, 0.5f));
        float3 offsetY = MipUniforms.texelSize.xyz * float3(0.0f, 0.5f, 0.0f);
        return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetY).xy));
    #elif NON_POWER_OF_TWO == 3
        // > 2:1 ratio in X and Y
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.25f, 0.5f));
        float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0.5f, 0.0f);
        TEXTURE_TYPE sample = SampleAt(coord, uvw.xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, offset.y, 0.0f)).xy);
        return sample * 0.25f;
    #elif NON_POWER_OF_TWO == 4
        // > 2:1 ratio in Z only
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.5f, 0.25f));
        float3 offsetZ = MipUniforms.texelSize.xyz * float3(0.0f, 0.0f, 0.5f);
        return 0.5f * (SampleAt(coord, uvw.xy) + SampleAt(coord, (uvw + offsetZ).xy));
    #elif NON_POWER_OF_TWO == 5
        // > 2:1 ratio in X and Z
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.25f, 0.5f, 0.25f));
        float3 offset = MipUniforms.texelSize.xyz * float3(0.5f, 0.0f, 0.5f);
        TEXTURE_TYPE sample = SampleAt(coord, uvw.xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, 0.0f, offset.z)).xy);
        return sample * 0.25f;
    #elif NON_POWER_OF_TWO == 6
        // > 2:1 ratio in Y and Z
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + float3(0.5f, 0.25f, 0.25f));
        float3 offset = MipUniforms.texelSize.xyz * float3(0.0f, 0.5f, 0.5f);
        TEXTURE_TYPE sample = SampleAt(coord, uvw.xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, offset.y, offset.z)).xy);
        return sample * 0.25f;
    #elif NON_POWER_OF_TWO == 7
        // > 2:1 ratio in all dimensions (X, Y, Z)
        float3 uvw = MipUniforms.texelSize.xyz * (coord.xyz + 0.25f);
        float3 offset = MipUniforms.texelSize.xyz * 0.5f;
        TEXTURE_TYPE sample = SampleAt(coord, uvw.xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, 0.0f, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, offset.y, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, offset.y, 0.0f)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, 0.0f, offset.z)).xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, 0.0f, offset.z)).xy);
        sample += SampleAt(coord, (uvw + float3(0.0f, offset.y, offset.z)).xy);
        sample += SampleAt(coord, (uvw + float3(offset.x, offset.y, offset.z)).xy);
        return sample * 0.125f;
    #endif
    
#else // 2D, 2DArray, Cube, CubeArray
    #if NON_POWER_OF_TWO == 0
        float2 uv = MipUniforms.texelSize.xy * (coord.xy + 0.5f);
        return SampleAt(coord, uv);
    #elif NON_POWER_OF_TWO == 1
        float2 uv = MipUniforms.texelSize.xy * (coord.xy + float2(0.25f, 0.5f));
        float2 offset = MipUniforms.texelSize.xy * float2(0.5f, 0.0f);
        return 0.5f * (SampleAt(coord, uv) + SampleAt(coord, uv + offset));
    #elif NON_POWER_OF_TWO == 2
        float2 uv = MipUniforms.texelSize.xy * (coord.xy + float2(0.5f, 0.25f));
        float2 offset = MipUniforms.texelSize.xy * float2(0.0f, 0.5f);
        return 0.5f * (SampleAt(coord, uv) + SampleAt(coord, uv + offset));
    #elif NON_POWER_OF_TWO == 3
        float2 uv = MipUniforms.texelSize.xy * (coord.xy + 0.25f);
        float2 offset = MipUniforms.texelSize.xy * 0.5f;
        TEXTURE_TYPE sample = SampleAt(coord, uv);
        sample += SampleAt(coord, uv + float2(offset.x, 0.0f));
        sample += SampleAt(coord, uv + float2(0.0f, offset.y));
        sample += SampleAt(coord, uv + offset);
        return sample * 0.25f;
    #endif
#endif
}

void WriteMip0(uint3 coord, TEXTURE_TYPE color)
{
#if TEXTURE_DIMENSION == 0 // Texture2D
    RWTexture2D<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
    dst[coord.xy] = color;
#else // All array/cube/3D types use Texture2DArray or RWTexture3D
#if TEXTURE_DIMENSION == 4 // Texture3D
    RWTexture3D<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
    dst[coord.xyz] = color;
#else
    RWTexture2DArray<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle0);
    dst[coord.xyz] = color;
#endif
#endif
}

void WriteMip1(uint3 coord, TEXTURE_TYPE color)
{
#if TEXTURE_DIMENSION == 0 // Texture2D
    RWTexture2D<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
    dst[coord.xy >> 1] = color;
#else
#if TEXTURE_DIMENSION == 4 // Texture3D
    RWTexture3D<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
    dst[coord.xyz >> 1] = color;
#else
    RWTexture2DArray<TEXTURE_TYPE> dst = GetBindlessResource(MipUniforms.destinationMipHandle1);
    dst[uint3(coord.xy >> 1, coord.z)] = color;
#endif
#endif
}

#if TEXTURE_DIMENSION == TEXTURE_DIMENSION_3D
    #define THREADGROUP_SIZE_X 4
    #define THREADGROUP_SIZE_Y 4
    #define THREADGROUP_SIZE_Z 4
#else
    #define THREADGROUP_SIZE_X 8
    #define THREADGROUP_SIZE_Y 8
    #define THREADGROUP_SIZE_Z 1
#endif

[numthreads(THREADGROUP_SIZE_X, THREADGROUP_SIZE_Y, THREADGROUP_SIZE_Z)]
void MipGenerationCS(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    // Sample and downsample first mip
    TEXTURE_TYPE sampleCenter = SampleWithNPOT(dtid);
    
    // Write first output mip
    WriteMip0(dtid, PackColor<TEXTURE_TYPE>(sampleCenter));
    
    // Early exit if we're only generating one mip
    if (MipUniforms.numMips == 1)
    {
        return;
    }
    
    // Generate second mip using wave intrinsics
    // ddx() is temporary, it forces the SPV_KHR_compute_shader_derivatives extension to be emitted by DXC, allowing for threads to be grouped in 2x2 quads.
    // An issue was opened on the DXC repo: https://github.com/microsoft/DirectXShaderCompiler/issues/7943
    TEXTURE_TYPE sampleRight = TEXTURE_TYPE(ddx(sampleCenter) * 0.00001f + QuadReadAcrossX(sampleCenter));
    TEXTURE_TYPE sampleDown = QuadReadAcrossY(sampleCenter);
    TEXTURE_TYPE sampleDiag = QuadReadAcrossDiagonal(sampleCenter);
    TEXTURE_TYPE dstSample = TEXTURE_TYPE(0.25f * (sampleCenter + sampleRight + sampleDown + sampleDiag));
    
    // Only one thread per 2x2 quad writes to second mip
    bool shouldWriteMip1 = ((gtid.x & 1) == 0) && ((gtid.y & 1) == 0);
    if (shouldWriteMip1)
    {
        WriteMip1(dtid, PackColor<TEXTURE_TYPE>(dstSample));
    }
}

)SHADER";

static const ShaderKey MipGenerationShaderKey{
    .sourceCode = std::string(MipGenerationSource),
    .entryPoint = std::string(MipGenerationEntryPoint),
    .type = ShaderType::ComputeShader,
    .compiler = ShaderCompilerBackend::DXC,
};

} // namespace vex
