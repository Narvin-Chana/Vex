#include <array>

#include <Vex.h>

// Compiles and dispatches a compute shader, which requires Vex, its dependencies and the files deployed by
// vex_setup_runtime (DLLs, Vex.hlsli...) to all be usable from outside of Vex's own build.
int main()
{
    vex::Graphics graphics{ vex::GraphicsCreateDesc{ .useSwapChain = false } };

    vex::ShaderCompiler shaderCompiler;

    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);
    ctx.Dispatch(shaderCompiler.GetShaderView(vex::ShaderKey{
                     .filepath = SHADER_PATH,
                     .entryPoint = "CSMain",
                     .type = vex::ShaderType::ComputeShader,
                 }),
                 {},
                 {},
                 std::array{ 1u, 1u, 1u });
    graphics.Submit(ctx);

    return 0;
}
