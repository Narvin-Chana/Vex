#include <ExamplePaths.h>

#include <Vex/CommandContext.h>
#include <Vex/Graphics.h>

int main()
{
    const uint32_t width = 500;
    const uint32_t height = 500;

    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
    } };

    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);

    ctx.Dispatch(
        vex::ShaderKey{
            .path = ExamplesDir / "hello_windowless/Dummy.hlsl",
            .entryPoint = "CSMain",
            .type = vex::ShaderType::ComputeShader,
        },
        {},
        std::array{
            (width + 7u) / 8u,
            (height + 7u) / 8u,
            1u,
        });

    graphics.Submit(ctx);
}
