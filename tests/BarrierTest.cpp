#include "VexTest.h"

#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace vex
{

struct BarrierTest : VexTest
{
    static constexpr u32 ElementCount = 256;
    static constexpr u32 ElementByteSize = sizeof(u32);
    static constexpr u32 ThreadGroupSize = 64;
    static constexpr u32 SlowIterationCount = 1u << 20;

    static constexpr u32 InitialSeed = 0;
    static constexpr u32 FirstSeed = 1000;
    static constexpr u32 SecondSeed = 2000;

    struct ShaderUniforms
    {
        BindlessHandle inputBuffer;
        BindlessHandle outputBuffer;
        u32 iterations{};
        u32 seed{};
        u32 zero{};
    };

    Buffer CreateTestBuffer(std::string name)
    {
        return graphics.CreateBuffer(BufferDesc{
            .name = std::move(name),
            .byteSize = ElementCount * ElementByteSize,
            .usage = BufferUsage::ShaderRead | BufferUsage::ShaderReadWrite,
        });
    }

    ShaderView GetShader(std::string entryPoint)
    {
        return shaderCompiler.GetShaderView({
            .filepath = (VexRootPath / "tests/shaders/BarrierTest.hlsl").string(),
            .entryPoint = std::move(entryPoint),
            .type = ShaderType::ComputeShader,
        });
    }

    // Writes (index + seed) to every element of the output once the dispatch has spun for the given iterations.
    void Produce(CommandContext& ctx, const Buffer& output, u32 seed, u32 iterations, bool trackOutput = true)
    {
        const BufferBinding outputBinding = BufferBinding::CreateRWStructured(output, ElementByteSize);
        const std::array<ResourceBinding, 1> bindings{ outputBinding };
        const ShaderUniforms uniforms{
            .outputBuffer = graphics.GetBindlessHandle(outputBinding),
            .iterations = iterations,
            .seed = seed,
        };

        ctx.Dispatch(GetShader("CSProduce"),
                     ConstantBinding(std::span{ &uniforms, 1 }),
                     trackOutput ? Span<const ResourceBinding>(bindings) : Span<const ResourceBinding>{},
                     {
                         ElementCount / ThreadGroupSize,
                         1u,
                         1u,
                     });
    }

    // Copies the input to the output reading the input only after the dispatch has spun for the given number of iterations.
    void Consume(CommandContext& ctx, const Buffer& input, const Buffer& output, u32 iterations)
    {
        const ShaderUniforms uniforms{
            .inputBuffer = graphics.GetBindlessHandle(BufferBinding::CreateStructured(input, ElementByteSize)),
            .outputBuffer = graphics.GetBindlessHandle(BufferBinding::CreateRWStructured(output, ElementByteSize)),
            .iterations = iterations,
        };

        ctx.Dispatch(GetShader("CSConsume"),
                     ConstantBinding(std::span{ &uniforms, 1 }),
                     {},
                     {
                         ElementCount / ThreadGroupSize,
                         1u,
                         1u,
                     });
    }

    void TestCrossQueueReadAfterWrite(QueueType producerQueue, QueueType consumerQueue)
    {
        Buffer source = CreateTestBuffer("Source");
        Buffer destination = CreateTestBuffer("Destination");

        CommandContext producerCtx = graphics.CreateCommandContext(producerQueue);
        Produce(producerCtx, source, FirstSeed, SlowIterationCount);

        CommandContext consumerCtx = graphics.CreateCommandContext(consumerQueue);
        // Copy queue can't dispatch, so it reads back the source directly.
        if (consumerQueue != QueueType::Copy)
        {
            Consume(consumerCtx, source, destination, 0);
        }
        BufferReadbackContext readbackContext =
            consumerCtx.EnqueueDataReadback(consumerQueue == QueueType::Copy ? source : destination);

        const SyncToken producerToken = graphics.Submit(producerCtx);
        graphics.WaitForTokenOnCPU(graphics.Submit(consumerCtx, { &producerToken, 1 }));

        EXPECT_EQ(ReadData(readbackContext), CreateData(FirstSeed));
    }

    static constexpr std::vector<u32> CreateData(u32 seed)
    {
        std::vector<u32> data(ElementCount);
        std::iota(data.begin(), data.end(), seed);
        return data;
    }

    static std::vector<u32> ReadData(const BufferReadbackContext& readbackContext)
    {
        std::vector<u32> data(ElementCount);
        readbackContext.ReadData(std::as_writable_bytes(std::span{ data }));
        return data;
    }
};

TEST_F(BarrierTest, ReadAfterTrackedWrite)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);

    Buffer source = CreateTestBuffer("Source");
    Buffer destination = CreateTestBuffer("Destination");

    // Produce takes a while, Consume is done as fast as possible (0 iterations).
    // If the barrier is correctly emitted, Consume finishes AFTER Produce.
    Produce(ctx, source, FirstSeed, SlowIterationCount);
    Consume(ctx, source, destination, 0);

    BufferReadbackContext readbackContext = ctx.EnqueueDataReadback(destination);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadData(readbackContext), CreateData(FirstSeed));
}

TEST_F(BarrierTest, WriteAfterUntrackedRead)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);

    Buffer source = CreateTestBuffer("Source");
    Buffer destination = CreateTestBuffer("Destination");

    const std::vector<u32> initialData = CreateData(InitialSeed);
    ctx.EnqueueDataUpload(source, std::as_bytes(std::span{ initialData }));

    // The overwrite must wait for the slow consumer to have read the initial data.
    // Consume is slow, produce is fast.
    // This means the barrier is correctly setup if Produce doesn't change the result of the copy Consume does to "destination".
    Consume(ctx, source, destination, SlowIterationCount);
    Produce(ctx, source, FirstSeed, 0);

    BufferReadbackContext destinationReadback = ctx.EnqueueDataReadback(destination);
    BufferReadbackContext sourceReadback = ctx.EnqueueDataReadback(source);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadData(destinationReadback), initialData);
    EXPECT_EQ(ReadData(sourceReadback), CreateData(FirstSeed));
}

TEST_F(BarrierTest, WriteAfterWrite)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);

    Buffer buffer = CreateTestBuffer("Buffer");

    Produce(ctx, buffer, FirstSeed, SlowIterationCount);
    Produce(ctx, buffer, SecondSeed, 0);

    BufferReadbackContext readbackContext = ctx.EnqueueDataReadback(buffer);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadData(readbackContext), CreateData(SecondSeed));
}

TEST_F(BarrierTest, ExplicitBarrierAfterUntrackedWrite)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);

    Buffer source = CreateTestBuffer("Source");
    Buffer destination = CreateTestBuffer("Destination");

    // Untracked writes are invisible to Vex (it has no way of knowing), we must manually synchronize them.
    Produce(ctx, source, FirstSeed, SlowIterationCount, false);
    ctx.Barrier(source, RHIBarrierAccess::ShaderRead);
    Consume(ctx, source, destination, 0);

    BufferReadbackContext readbackContext = ctx.EnqueueDataReadback(destination);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_EQ(ReadData(readbackContext), CreateData(FirstSeed));
}

TEST_F(BarrierTest, CrossQueueComputeToGraphics)
{
    TestCrossQueueReadAfterWrite(QueueType::Compute, QueueType::Graphics);
}

TEST_F(BarrierTest, CrossQueueGraphicsToCompute)
{
    TestCrossQueueReadAfterWrite(QueueType::Graphics, QueueType::Compute);
}

TEST_F(BarrierTest, CrossQueueComputeToCopy)
{
    TestCrossQueueReadAfterWrite(QueueType::Compute, QueueType::Copy);
}

} // namespace vex
