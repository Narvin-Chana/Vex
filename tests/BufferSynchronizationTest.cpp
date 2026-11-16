#include "VexTest.h"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace vex
{

struct BufferSynchronizationTest : VexTest
{
    static constexpr u32 ElementCount = 64;
    static constexpr u32 SlowIterations = 1u << 20;

    Buffer CreateTestBuffer(const std::string& name, u32 elementCount = ElementCount)
    {
        return graphics.CreateBuffer(BufferDesc::CreateGenericBufferDesc(name, sizeof(u32) * elementCount, true));
    }

    void Dispatch(CommandContext& ctx,
                  bool consume,
                  const Buffer& input,
                  const Buffer& output,
                  u32 seed,
                  u32 iterations,
                  u32 offset = 0,
                  bool trackOutput = true)
    {
        const BufferBinding outputBinding = BufferBinding::CreateRWStructured<u32>(output);
        struct Uniforms
        {
            BindlessHandle inputHandle;
            BindlessHandle outputHandle;
            u32 seed;
            u32 iterations;
            u32 offset;
            u32 mask;
        } uniforms{
            .inputHandle = graphics.GetBindlessHandle(BufferBinding::CreateStructured<u32>(input)),
            .outputHandle = graphics.GetBindlessHandle(outputBinding),
            .seed = seed,
            .iterations = iterations,
            .offset = offset,
            .mask = 0,
        };

        const std::array<ResourceBinding, 1> trackedResources{ outputBinding };
        ctx.Dispatch(shaderCompiler.GetShaderView({
                         .filepath = (TestShaderPath / "BufferSynchronization.cs.hlsl").string(),
                         .entryPoint = "CSMain",
                         .type = ShaderType::ComputeShader,
                         .defines = { { "CONSUME", consume ? "1" : "0" } },
                     }),
                     ConstantBinding(uniforms),
                     trackOutput ? Span<const ResourceBinding>(trackedResources) : Span<const ResourceBinding>{},
                     { 1u, 1u, 1u });
    }

    void Produce(CommandContext& ctx, const Buffer& output, u32 seed, u32 iterations, u32 offset = 0)
    {
        Dispatch(ctx, false, output, output, seed, iterations, offset);
    }

    void Consume(CommandContext& ctx, const Buffer& input, const Buffer& output, u32 delayIterations, u32 offset = 0)
    {
        Dispatch(ctx, true, input, output, 0, delayIterations, offset);
    }

    // Both contexts are recorded before submitting either, so the consumer is submitted while the producer is still
    // running and only the queue dependency orders them.
    std::vector<u32> RunCrossQueue(QueueType producerQueue, QueueType consumerQueue, u32 seed)
    {
        const Buffer produced = CreateTestBuffer("Produced");
        const Buffer consumed = CreateTestBuffer("Consumed");

        CommandContext producerCtx = graphics.CreateCommandContext(producerQueue);
        Produce(producerCtx, produced, seed, SlowIterations);

        // Copy queues cannot dispatch, they read back the produced buffer directly.
        const bool consumerDispatches = consumerQueue != QueueType::Copy;
        CommandContext consumerCtx = graphics.CreateCommandContext(consumerQueue);
        if (consumerDispatches)
        {
            Consume(consumerCtx, produced, consumed, 0);
        }
        const BufferReadbackContext readback = consumerCtx.EnqueueDataReadback(consumerDispatches ? consumed : produced);

        const SyncToken produceToken = graphics.Submit(producerCtx);
        graphics.WaitForTokenOnCPU(graphics.Submit(consumerCtx, { &produceToken, 1 }));
        return ReadValues(readback);
    }

    static std::vector<u32> ExpectedValues(u32 seed, u32 iterations, u32 increment = 0)
    {
        std::vector<u32> values(ElementCount);
        for (u32 i = 0; i < ElementCount; ++i)
        {
            u32 value = seed + i;
            for (u32 iteration = 0; iteration < iterations; ++iteration)
            {
                value = value * 1664525u + 1013904223u;
            }
            values[i] = value + increment;
        }
        return values;
    }

    static std::vector<u32> ReadValues(const BufferReadbackContext& readback, u32 elementCount = ElementCount)
    {
        std::vector<u32> values(elementCount);
        readback.ReadData(std::as_writable_bytes(std::span{ values }));
        return values;
    }
};

TEST_F(BufferSynchronizationTest, CrossQueueComputeToCopy)
{
    EXPECT_EQ(RunCrossQueue(QueueType::Compute, QueueType::Copy, 7), ExpectedValues(7, SlowIterations));
}

TEST_F(BufferSynchronizationTest, CrossQueueComputeToGraphics)
{
    EXPECT_EQ(RunCrossQueue(QueueType::Compute, QueueType::Graphics, 11), ExpectedValues(11, SlowIterations, 1));
}

TEST_F(BufferSynchronizationTest, CrossQueueGraphicsToCompute)
{
    EXPECT_EQ(RunCrossQueue(QueueType::Graphics, QueueType::Compute, 13), ExpectedValues(13, SlowIterations, 1));
}

TEST_F(BufferSynchronizationTest, ReadAfterTrackedWrite)
{
    const Buffer produced = CreateTestBuffer("Produced");
    const Buffer consumed = CreateTestBuffer("Consumed");

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);
    Produce(ctx, produced, 17, SlowIterations);
    // The consumer reads the produced buffer without tracking it, the tracked write must still be waited for.
    Consume(ctx, produced, consumed, 0);
    const BufferReadbackContext readback = ctx.EnqueueDataReadback(consumed);
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadValues(readback), ExpectedValues(17, SlowIterations, 1));
}

TEST_F(BufferSynchronizationTest, WriteAfterUntrackedRead)
{
    const Buffer produced = CreateTestBuffer("Produced");
    const Buffer consumed = CreateTestBuffer("Consumed");

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);
    Produce(ctx, produced, 19, 16);
    // The consumer only reads its (untracked) input after a long loop.
    Consume(ctx, produced, consumed, SlowIterations);
    // This fast overwrite must wait for the consumer's read.
    Produce(ctx, produced, 23, 0);
    const BufferReadbackContext readback = ctx.EnqueueDataReadback(consumed);
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadValues(readback), ExpectedValues(19, 16, 1));
}

TEST_F(BufferSynchronizationTest, ExplicitBarrierForUntrackedWrite)
{
    const Buffer produced = CreateTestBuffer("Produced");
    const Buffer consumed = CreateTestBuffer("Consumed");

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);
    // The write is not tracked, so the user is responsible for synchronizing it.
    Dispatch(ctx, false, produced, produced, 37, SlowIterations, 0, false);
    ctx.Barrier(produced, RHIBarrierAccess::ShaderRead);
    Consume(ctx, produced, consumed, 0);
    const BufferReadbackContext readback = ctx.EnqueueDataReadback(consumed);
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadValues(readback), ExpectedValues(37, SlowIterations, 1));
}

} // namespace vex
