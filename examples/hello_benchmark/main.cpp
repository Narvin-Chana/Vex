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

    // Begins a timestamp for
    vex::QueryHandle globalQueryHandle = ctx.BeginTimestampQuery();
    vex::QueryHandle dispatchQueries[N]{};
    for (auto i = 0; i < N; ++i)
    {
        dispatchQueries[i] = ctx.BeginTimestampQuery();

        // Create the bindings and obtain the bindless handles we need for our compute passes.
        vex::ResourceBinding passBindings[]{
            vex::BufferBinding{
                .buffer = resultBuffer,
                .usage = vex::BufferBindingUsage::RWStructuredBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(float) * M),
            },
        };
        std::vector<vex::BindlessHandle> passHandles = ctx.GetBindlessHandles(passBindings);

        ctx.TransitionBindings(passBindings);

        ctx.Dispatch(
            vex::ShaderKey{
                .path = ExamplesDir / "hello_benchmark/Dummy.cs.hlsl",
                .entryPoint = "CSMain",
                .type = vex::ShaderType::ComputeShader,
            },
            vex::ConstantBinding{ std::span{ passHandles } },
            std::array{ M / 8, 1u, 1u });

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
