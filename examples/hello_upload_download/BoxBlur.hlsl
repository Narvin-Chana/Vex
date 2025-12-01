#include <Vex.hlsli>

struct UniformStruct
{
    uint inputTextureHandle;
    uint outputTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const Texture2D<float4> InputTexture = GetBindlessResource(Uniforms.inputTextureHandle);
static const RWTexture2D<float4> OutputTexture = GetBindlessResource(Uniforms.outputTextureHandle);
static const int blurSize = 3;

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);

    float4 value = 0.0f.xxxx;

    for (int y = -blurSize; y <= blurSize; ++y) {
        for (int x = -blurSize; x <= blurSize; ++x) {
            int2 coord = (int2)dtid.xy + int2(x, y);
            if (coord.x < 0 || coord.x >= width || coord.y < 0 || coord.y >= height)
            {
                continue;
            }

            value += float4(InputTexture[coord].rgb, 1);
        }
    }

    OutputTexture[dtid.xy / 2] = float4(value.rgb / value.a, 1);
}
