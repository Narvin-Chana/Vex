#include <array>
#include <optional>
#include <print>
#include <string>

#include <VexShaderCompiler.h>

// Compiles and dispatches a compute shader, which requires the runtime, the shader compiler and their deployed
// dependencies (DLLs, Vex.hlsli...) to all be usable from outside of Vex's own build.
int main()
{
    vex::Graphics graphics{ vex::GraphicsCreateDesc{ .useSwapChain = false } };

    vex::ShaderCompiler shaderCompiler;
    const vex::ShaderKey shaderKey{
        .filepath = SHADER_PATH,
        .entryPoint = "CSMain",
        .type = vex::ShaderType::ComputeShader,
    };
    if (std::optional<std::string> error = shaderCompiler.CompileShaderFromFilepath(shaderKey))
    {
        std::println("Shader compilation failed: {}", *error);
        return 1;
    }

    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);
    ctx.Dispatch(shaderCompiler.GetShaderView(shaderKey), {}, {}, std::array{ 1u, 1u, 1u });
    graphics.Submit(ctx);

    return 0;
}
