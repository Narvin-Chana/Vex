import Vex;

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

[numthreads(1, 1, 1)]
void CSMain(
    uniform uint testBufferHandle,
    uniform uint outputBufferHandle,
    uniform uint numElements
)
{
    static let TestInfoBuffer = GetBindlessResource<BUFFER_TYPE>(testBufferHandle);
    static let OutputBuffer = GetBindlessResource<RWStructuredBuffer<float3>>(outputBufferHandle);

    float3 total = float3(0,0,0);
#if CONSTANT_BUFFER
    total = (*TestInfoBuffer).v;
#else
    for (uint i = 0; i < numElements; ++i)
    {
        #if BYTE_ADDRESS_BUFFER
        let bufferData = TestInfoBuffer.Load<TestBufferData>(i * sizeof(TestBufferData));
        #else
        let bufferData = TestInfoBuffer.Load((int)i);
        #endif
        total += bufferData.v;
    }
#endif
    OutputBuffer[0] += total;
}