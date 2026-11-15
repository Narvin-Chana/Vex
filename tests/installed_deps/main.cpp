#include <array>
#include <optional>
#include <print>
#include <string>

#include <Vex.h>

// Compiles a compute shader with each compiler backend and dispatches it, so DXC, Slang and SPIRV-Reflect (used for
// the reflection of SPIR-V shaders) all get exercised through the installed packages.
int main()
{
    vex::Graphics graphics{ vex::GraphicsCreateDesc{ .useSwapChain = false } };
    vex::ShaderCompiler shaderCompiler;

    const std::array shaderKeys{
        vex::ShaderKey{ .filepath = SHADER_DIR "/Compute.hlsl",
                        .entryPoint = "CSMain",
                        .type = vex::ShaderType::ComputeShader,
                        .compiler = vex::ShaderCompilerBackend::DXC },
        vex::ShaderKey{ .filepath = SHADER_DIR "/Compute.slang",
                        .entryPoint = "CSMain",
                        .type = vex::ShaderType::ComputeShader,
                        .compiler = vex::ShaderCompilerBackend::Slang },
    };

    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);
    for (const vex::ShaderKey& shaderKey : shaderKeys)
    {
        if (std::optional<std::string> error = shaderCompiler.CompileShaderFromFilepath(shaderKey))
        {
            std::println("Compilation of {} failed: {}", shaderKey.GetDebugName(), *error);
            return 1;
        }
        ctx.Dispatch(shaderCompiler.GetShaderView(shaderKey), {}, {}, std::array{ 1u, 1u, 1u });
        std::println("Compiled and dispatched {}", shaderKey.GetDebugName());
    }
    graphics.Submit(ctx);

    return 0;
}
