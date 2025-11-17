#include <Vex.hlsli>

struct UniformStruct
{
    uint testBufferHandle;
    uint outputBufferHandle;

    uint numElements;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

struct TestBufferData
{
    float3 v;
};

#if CONSTANT_BUFFER
    #define BUFFER_TYPE ConstantBuffer<TestBufferData>
#elif STRUCTURED_BUFFER
    #if READ_WRITE
        #define BUFFER_TYPE RWStructuredBuffer<TestBufferData>
    #else
        #define BUFFER_TYPE StructuredBuffer<TestBufferData>
    #endif
#elif BYTE_ADDRESS_BUFFER
    #if READ_WRITE
        #define BUFFER_TYPE RWByteAddressBuffer
    #else
        #define BUFFER_TYPE ByteAddressBuffer
    #endif
#endif

static BUFFER_TYPE TestInfoBuffer = GetBindlessResource(Uniforms.testBufferHandle);
static RWStructuredBuffer<float3> OutputBuffer = GetBindlessResource(Uniforms.outputBufferHandle);

[numthreads(1, 1, 1)]
void CSMain()
{
    float3 total = float3(0,0,0);
#if CONSTANT_BUFFER
    total = TestInfoBuffer.v;
#else
    for (uint i = 0; i < Uniforms.numElements; ++i)
    {
        #if BYTE_ADDRESS_BUFFER
        TestBufferData bufferData = TestInfoBuffer.Load<TestBufferData>(i * sizeof(TestBufferData));
        #else
        TestBufferData bufferData = TestInfoBuffer.Load((int)i);
        #endif
        total += bufferData.v;
    }
#endif
    OutputBuffer[0] = total;
}