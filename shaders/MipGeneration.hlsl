#include <Vex.hlsli>

#ifndef TEXTURE_TYPE
// Fallback to float4
#define TEXTURE_TYPE float4
#endif

#ifndef LINEAR_SAMPLER_SLOT
#define LINEAR_SAMPLER_SLOT s0
#endif

SamplerState LinearSampler : register(LINEAR_SAMPLER_SLOT);

struct Uniforms
{
    uint sourceMipHandle;
    uint destinationMipHandle;
    float2 texelSize; // 1.0f / (mip0 texture size)
    uint sourceMipLevel;
};

VEX_UNIFORMS(Uniforms, MipUniforms);

[numthreads(8, 8, 1)]
void MipGenerationTexture2D(uint3 dtid : SV_DispatchThreadID)
{
    float2 uv = (float2) dtid.xy + 0.5f.xx;
    uv *= MipUniforms.texelSize;
    
    Texture2D<TEXTURE_TYPE> sourceMip = GetBindlessResource(MipUniforms.sourceMipHandle);
    RWTexture2D<TEXTURE_TYPE> destinationMip = GetBindlessResource(MipUniforms.destinationMipHandle);
    
    // TODO: a linear sample won't work with integer types!
    
    TEXTURE_TYPE sample = sourceMip.SampleLevel(LinearSampler, uv, MipUniforms.sourceMipLevel);
    destinationMip[dtid.xy] = sample;
}