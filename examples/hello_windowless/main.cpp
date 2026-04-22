#include <ExamplePaths.h>

#if VEX_MODULES
import Vex;
#else
#include <Vex.h>
#endif

int main()
{
    constexpr int width = 500;
    constexpr int height = 500;

    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
    } };

    vex::ShaderCompiler shaderCompiler;

    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);

    ctx.Dispatch(shaderCompiler.GetShaderView(vex::ShaderKey{
                     .filepath = (ExamplesDir / "hello_windowless/Dummy.hlsl").string(),
                     .entryPoint = "CSMain",
                     .type = vex::ShaderType::ComputeShader,
                 }),
                 {},
                 {},
                 std::array{
                     (width + 7u) / 8u,
                     (height + 7u) / 8u,
                     1u,
                 });

    graphics.Submit(ctx);
}
