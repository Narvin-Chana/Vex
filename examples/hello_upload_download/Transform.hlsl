#include <Vex.hlsli>

struct UniformStruct
{
    uint outputTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const RWTexture2D<float4> OutputTexture = GetBindlessResource(Uniforms.outputTextureHandle);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    OutputTexture.GetDimensions(width, height);

    float2 globalUV = float2(dtid.xy) / max(width, height);
    OutputTexture[dtid.xy].rgb = 1.xxx - OutputTexture[dtid.xy].rgb;
}
