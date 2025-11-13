#include <Vex.hlsli>

struct UniformStruct
{
    uint resultBufferHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

// HLSL way of declaring global resources:
static const RWStructuredBuffer<float> ResultBuffer = GetBindlessResource(Uniforms.resultBufferHandle);

[numthreads(8, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    float value = ResultBuffer[dtid.x];

    [loop] for (int i = 0; i < 100000; ++i)
    {
        value += 1;
    }

    ResultBuffer[dtid.x] = value;
}
