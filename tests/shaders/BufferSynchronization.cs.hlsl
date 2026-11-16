#include <Vex.hlsli>

struct UniformStruct
{
    uint inputHandle;
    uint outputHandle;
    uint seed;
    uint iterations;
    uint offset;
    // Always zero, used to make the consumer's read depend on its loop without the compiler being able to fold it.
    uint mask;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

static const StructuredBuffer<uint> Input = GetBindlessResource(Uniforms.inputHandle);
static const RWStructuredBuffer<uint> Output = GetBindlessResource(Uniforms.outputHandle);

uint Lcg(uint value, uint iterations)
{
    [loop] for (uint i = 0; i < iterations; ++i)
    {
        value = value * 1664525u + 1013904223u;
    }
    return value;
}

// A single entry point selected with a define: two entry points of the same file would share a PSO, as the DXC
// shader hash does not include the entry point.
[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    const uint index = Uniforms.offset + dtid.x;
#if CONSUME
    // Reads its input only after a long dependency chain, so its reads land late on the GPU timeline.
    const uint delay = Lcg(dtid.x, Uniforms.iterations);
    Output[index] = Input[index + (delay & Uniforms.mask)] + 1;
#else
    // Writes its result only after a long dependency chain, so its writes land late on the GPU timeline.
    Output[index] = Lcg(Uniforms.seed + dtid.x, Uniforms.iterations);
#endif
}
