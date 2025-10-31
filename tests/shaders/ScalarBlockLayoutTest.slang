import Vex;

struct WeirdlyPackedData
{
    float3 v1;
    float3 v2;
    float3 v3;
};

[shader("compute")]
[numthreads(1, 1, 1)]
void CSMain(
    uint3 dtid : SV_DispatchThreadID,
    uniform uint InputBufferHandle,
    uniform uint ResultBufferHandle)
{
    let InputBuffer = GetBindlessResource<StructuredBuffer<WeirdlyPackedData>>(InputBufferHandle);
    let ResultBuffer = GetBindlessResource<RWStructuredBuffer<float3>>(ResultBufferHandle);

    float3 total = float3(0,0,0);
    for (int i = 0; i < 13; ++i)
    {
        WeirdlyPackedData data = InputBuffer.Load(i);
        total += data.v1 + data.v2 + data.v3;
    }
    ResultBuffer[0] = total;
}
