#include <ExamplePaths.h>

#include <Vex/CommandContext.h>
#include <Vex/GfxBackend.h>

int main()
{
    const uint32_t width = 500;
    const uint32_t height = 500;

    vex::GfxBackend backend{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
    } };

    {
        vex::CommandContext ctx =
            backend.BeginScopedCommandContext(vex::QueueType::Compute, vex::SubmissionPolicy::Immediate);

        ctx.Dispatch(
            vex::ShaderKey{
                .path = ExamplesDir / "hello_windowless/Dummy.hlsl",
                .entryPoint = "CSMain",
                .type = vex::ShaderType::ComputeShader,
            },
            std::nullopt,
            std::array{
                (width + 7u) / 8u,
                (height + 7u) / 8u,
                1u,
            });
    }
}
