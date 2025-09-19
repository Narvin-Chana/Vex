#include <Vex/CommandContext.h>
#include <Vex/GfxBackend.h>

int main()
{
    const uint32_t width = 500;
    const uint32_t height = 500;

    vex::GfxBackend backend{ vex::BackendDescription{
        .useSwapChain = false,
        .frameBuffering = vex::FrameBuffering::Triple,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
    } };

    {
        vex::CommandContext ctx =
            backend.BeginScopedCommandContext(vex::CommandQueueType::Compute, vex::SubmissionPolicy::Immediate);

        ctx.Dispatch(
            vex::ShaderKey{
                .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
                        "examples/hello_windowless/Dummy.hlsl",
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
