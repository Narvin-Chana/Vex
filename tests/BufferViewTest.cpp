#include "VexTest.h"

#include <gtest/gtest.h>

using namespace vex;

struct BufferBindingTestData
{
    BufferBindingUsage usage;
    std::optional<u64> offset;
    std::optional<u64> range;
    std::optional<u32> firstElement;
    std::optional<u32> elementCount;
    std::array<float, 3> expectedResult;
};

struct BufferBindingTest : public VexTestParam<BufferBindingTestData>
{
    static constexpr u32 ElementCount = 1024;
    static constexpr u32 DataComponentCount = 3;
    static constexpr u32 DataSize = sizeof(float) * DataComponentCount;
};

TEST_P(BufferBindingTest, CustomBindingOffset)
{
    auto ctx = graphics.BeginScopedCommandContext(QueueType::Compute, SubmissionPolicy::Immediate);

    std::vector<float> data;
    data.resize(DataComponentCount * ElementCount);
    for (int i = 0; i < data.size(); i += DataComponentCount)
    {
        for (int j = 0; j < DataComponentCount; ++j)
        {
            data[i + j] = static_cast<float>(j + 1);
        }
    }

    const BufferBindingTestData& testData = GetParam();
    BufferBindingUsage usage = testData.usage;

    Buffer dataBuffer = graphics.CreateBuffer(BufferDesc{
        .name = "DataBuffer",
        .byteSize = data.size() * sizeof(float),
        .usage = usage == BufferBindingUsage::ConstantBuffer
                     ? static_cast<u8>(BufferUsage::UniformBuffer)
                     : static_cast<u8>(BufferUsage::GenericBuffer | BufferUsage::ReadWriteBuffer),
    });
    Buffer resultBuffer =
        graphics.CreateBuffer(BufferDesc{ .name = "ResultBuffer",
                                          .byteSize = DataSize,
                                          .usage = BufferUsage::GenericBuffer | BufferUsage::ReadWriteBuffer });

    BufferBinding binding;
    switch (usage)
    {
    case BufferBindingUsage::ConstantBuffer:
        binding = BufferBinding::CreateConstantBuffer(dataBuffer, testData.offset.value_or(0), DataSize);
        break;
    case BufferBindingUsage::StructuredBuffer:
        binding = BufferBinding::CreateStructuredBuffer(dataBuffer,
                                                        DataSize,
                                                        testData.firstElement.value_or(0),
                                                        testData.elementCount);
        break;
    case BufferBindingUsage::RWStructuredBuffer:
        binding = BufferBinding::CreateRWStructuredBuffer(dataBuffer,
                                                          DataSize,
                                                          testData.firstElement.value_or(0),
                                                          testData.elementCount);
        break;
    case BufferBindingUsage::ByteAddressBuffer:
        binding = BufferBinding::CreateByteAddressBuffer(dataBuffer,
                                                         testData.firstElement.value_or(0),
                                                         testData.elementCount);
        break;
    case BufferBindingUsage::RWByteAddressBuffer:
        binding = BufferBinding::CreateRWByteAddressBuffer(dataBuffer,
                                                           testData.firstElement.value_or(0),
                                                           testData.elementCount);
        break;
    default:
        break;
    }

    ctx.EnqueueDataUpload(dataBuffer, std::as_bytes(std::span{ data }));

    ResourceBinding bindings[]{
        binding,
        BufferBinding::CreateRWStructuredBuffer(resultBuffer, DataSize),
    };
    std::vector<BindlessHandle> handles = graphics.GetBindlessHandles(bindings);

    struct Uniform
    {
        BindlessHandle inputBuffer;
        BindlessHandle outputBuffer;
        uint32_t numElements{};
    };

    Uniform uniforms{ handles[0],
                      handles[1],
                      usage == BufferBindingUsage::ConstantBuffer
                          ? 1
                          : GetParam().elementCount.value_or(ElementCount - testData.firstElement.value_or(0)) };

    ctx.BarrierBindings(bindings);

    ShaderKey key = {
        .path = VexRootPath / "tests/shaders/BufferView.cs.hlsl",
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .defines = {
            { "CONSTANT_BUFFER", usage == BufferBindingUsage::ConstantBuffer ? "1" : "0" },
            { "STRUCTURED_BUFFER", usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::RWStructuredBuffer ? "1" : "0" },
            { "BYTE_ADDRESS_BUFFER", usage == BufferBindingUsage::ByteAddressBuffer || usage == BufferBindingUsage::RWByteAddressBuffer ? "1" : "0" },
            { "READ_WRITE", usage == BufferBindingUsage::RWStructuredBuffer || usage == BufferBindingUsage::RWByteAddressBuffer ? "1" : "0" },
        },
    };

    ctx.Dispatch(key,
                 ConstantBinding(std::span{ &uniforms, 1 }),
                 {
                     1u,
                     1u,
                     1u,
                 });

    key.path = VexRootPath / "tests/shaders/BufferView.cs.slang";

    ctx.BarrierBindings(bindings);

    ctx.Dispatch(key,
                 ConstantBinding(std::span{ &uniforms, 1 }),
                 {
                     1u,
                     1u,
                     1u,
                 });

    auto readbackContext = ctx.EnqueueDataReadback(resultBuffer);

    graphics.WaitForTokenOnCPU(ctx.Submit());

    std::array<float, DataComponentCount> result{};

    readbackContext.ReadData(std::as_writable_bytes(std::span{ result }));

    std::array<float, 3> expectedResult{};
    std::transform(GetParam().expectedResult.begin(),
                   GetParam().expectedResult.end(),
                   expectedResult.begin(),
                   [](float f) { return f * 2; });

    EXPECT_TRUE(result == expectedResult);
}

INSTANTIATE_TEST_SUITE_P(StructureBufferTests,
                         BufferBindingTest,
                         testing::Values(BufferBindingTestData{ .usage = BufferBindingUsage::StructuredBuffer,
                                                                .expectedResult{
                                                                    1.f * BufferBindingTest::ElementCount,
                                                                    2.f * BufferBindingTest::ElementCount,
                                                                    3.f * BufferBindingTest::ElementCount,
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::StructuredBuffer,
                                                                .firstElement{ 4 },
                                                                .expectedResult{
                                                                    1.f * (BufferBindingTest::ElementCount - 4),
                                                                    2.f * (BufferBindingTest::ElementCount - 4),
                                                                    3.f * (BufferBindingTest::ElementCount - 4),
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::StructuredBuffer,
                                                                .firstElement{ 4 },
                                                                .elementCount{ 10 },
                                                                .expectedResult{
                                                                    1.f * 10,
                                                                    2.f * 10,
                                                                    3.f * 10,
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::StructuredBuffer,
                                                                .elementCount{ 100 },
                                                                .expectedResult{
                                                                    1.f * 100.f,
                                                                    2.f * 100.f,
                                                                    3.f * 100.f,
                                                                } }));

INSTANTIATE_TEST_SUITE_P(
    ConstantBufferTests,
    BufferBindingTest,
    testing::Values(BufferBindingTestData{ .usage = BufferBindingUsage::ConstantBuffer, .expectedResult{ 1, 2, 3 } },
                    BufferBindingTestData{
                        .usage = BufferBindingUsage::ConstantBuffer, .offset{ 256 }, .expectedResult{ 2, 3, 1 } },
                    BufferBindingTestData{
                        .usage = BufferBindingUsage::ConstantBuffer, .offset{ 512 }, .expectedResult{ 3, 1, 2 } },
                    BufferBindingTestData{
                        .usage = BufferBindingUsage::ConstantBuffer, .offset{ 768 }, .expectedResult{ 1, 2, 3 } }));

INSTANTIATE_TEST_SUITE_P(ByteAddressBufferTests,
                         BufferBindingTest,
                         testing::Values(BufferBindingTestData{ .usage = BufferBindingUsage::ByteAddressBuffer,
                                                                .expectedResult{
                                                                    1.f * BufferBindingTest::ElementCount,
                                                                    2.f * BufferBindingTest::ElementCount,
                                                                    3.f * BufferBindingTest::ElementCount,
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::ByteAddressBuffer,
                                                                .firstElement{ 3 },
                                                                .expectedResult{
                                                                    1.f * (BufferBindingTest::ElementCount - 4),
                                                                    2.f * (BufferBindingTest::ElementCount - 4),
                                                                    3.f * (BufferBindingTest::ElementCount - 4),
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::ByteAddressBuffer,
                                                                .firstElement{ 1 },
                                                                .elementCount{ 10 },
                                                                .expectedResult{
                                                                    2.f * 10.f,
                                                                    3.f * 10.f,
                                                                    1.f * 10.f,
                                                                } },
                                         BufferBindingTestData{ .usage = BufferBindingUsage::ByteAddressBuffer,
                                                                .elementCount{ 10 },
                                                                .expectedResult{
                                                                    1.f * 10.f,
                                                                    2.f * 10.f,
                                                                    3.f * 10.f,
                                                                } }));