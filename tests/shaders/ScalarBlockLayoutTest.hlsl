#include <Vex.hlsli>

struct WeirdlyPackedData
{
    float3 v1;
    float3 v2;
    float3 v3;
};

struct UniformStruct
{
    uint inputBufferHandle;
    uint resultBufferHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const StructuredBuffer<WeirdlyPackedData> InputBuffer = GetBindlessResource(Uniforms.inputBufferHandle);
static const RWStructuredBuffer<float3> ResultBuffer = GetBindlessResource(Uniforms.resultBufferHandle);

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    float3 total = float3(0,0,0);
    for (int i = 0; i < 13; ++i)
    {
        WeirdlyPackedData data = InputBuffer.Load(i);
        total += data.v1 + data.v2 + data.v3;
    }
    ResultBuffer[0] = total;
}
