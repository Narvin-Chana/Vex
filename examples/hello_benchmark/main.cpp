#include "ExamplePaths.h"

#include <Vex.h>

static constexpr vex::u32 N = 10;
static constexpr vex::u32 M = 1024;

int main()
{
    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
    } };

    vex::CommandContext ctx =
        graphics.BeginScopedCommandContext(vex::QueueType::Compute, vex::SubmissionPolicy::Immediate);

    vex::Buffer resultBuffer = graphics.CreateBuffer(
        vex::BufferDesc{ .name = "Result Buffer",
                         .byteSize = sizeof(float) * M,
                         .usage = vex::BufferUsage::GenericBuffer | vex::BufferUsage::ReadWriteBuffer,
                         .memoryLocality = vex::ResourceMemoryLocality::GPUOnly });

    // Begins a timestamp for the global command context
    vex::QueryHandle globalQueryHandle = ctx.BeginTimestampQuery();
    vex::QueryHandle dispatchQueries[N]{};

    // Create the binding for our output resource and obtain the bindless handle we need for our compute passes.
    vex::BufferBinding resultBufferBinding{
        .buffer = resultBuffer,
        .usage = vex::BufferBindingUsage::RWStructuredBuffer,
        .strideByteSize = static_cast<vex::u32>(sizeof(float)),
    };
    vex::BindlessHandle passHandle = graphics.GetBindlessHandle(resultBufferBinding);
    
    // Apply a barrier to allow for the resource to be written-to.
    ctx.BarrierBinding(resultBufferBinding);

    for (auto i = 0; i < N; ++i)
    {
        // Begins a timestamp query for iteration i
        dispatchQueries[i] = ctx.BeginTimestampQuery();

        ctx.Dispatch(
            vex::ShaderKey{
                .path = ExamplesDir / "hello_benchmark/Dummy.cs.hlsl",
                .entryPoint = "CSMain",
                .type = vex::ShaderType::ComputeShader,
            },
            vex::ConstantBinding{ passHandle },
            std::array{ M / 8, 1u, 1u });

        // Apply a barrier to flush the write we just performed.
        ctx.BarrierBinding(resultBufferBinding);

        ctx.EndTimestampQuery(dispatchQueries[i]);
    }
    ctx.EndTimestampQuery(globalQueryHandle);

    vex::SyncToken token = ctx.Submit();

    graphics.WaitForTokenOnCPU(token);

    vex::Query globalQuery = *graphics.GetTimestampValue(globalQueryHandle);
    for (auto i = 0; i < N; ++i)
    {
        vex::Query dispatchQuery = *graphics.GetTimestampValue(dispatchQueries[i]);
        VEX_LOG(vex::LogLevel::Info, "Dispatch {}: {:.5f}ms", i, dispatchQuery.durationMs);
    }
    VEX_LOG(vex::LogLevel::Info, "Total time: {:.5f}ms", globalQuery.durationMs);
}
