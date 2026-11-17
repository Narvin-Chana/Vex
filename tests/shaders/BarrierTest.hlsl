#include <Vex.hlsli>

struct UniformStruct
{
    uint inputBufferHandle;
    uint outputBufferHandle;
    uint iterations;
    uint seed;
    // We use this to make the shader actually perform the spin loop.
    uint zero;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static RWStructuredBuffer<uint> OutputBuffer = GetBindlessResource(Uniforms.outputBufferHandle);

// Keeps the thread busy for a configurable number of iterations.
uint Spin(uint x)
{
    for (uint i = 0; i < Uniforms.iterations; ++i)
    {
        x = x * 1664525u + 1013904223u;
    }
    return x;
}

[numthreads(64, 1, 1)]
void CSProduce(uint3 dtid : SV_DispatchThreadID)
{
    OutputBuffer[dtid.x] = dtid.x + Uniforms.seed + (Spin(dtid.x) & Uniforms.zero);
}

[numthreads(64, 1, 1)]
void CSConsume(uint3 dtid : SV_DispatchThreadID)
{
    StructuredBuffer<uint> InputBuffer = GetBindlessResource(Uniforms.inputBufferHandle);
    OutputBuffer[dtid.x] = InputBuffer[dtid.x + (Spin(dtid.x) & Uniforms.zero)];
}
