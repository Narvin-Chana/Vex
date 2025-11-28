#include "HelloRayTracing.h"

#include <GLFWIncludes.h>

HelloRayTracing::HelloRayTracing()
    : ExampleApplication("HelloRayTracing")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    SetupShaderErrorHandling();

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });
}

void HelloRayTracing::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        {
            auto ctx = graphics->BeginScopedCommandContext(vex::QueueType::Graphics);

            const vex::TextureBinding outputTextureBinding{
                .texture = workingTexture,
                .usage = vex::TextureBindingUsage::ShaderReadWrite,
            };

            ctx.TransitionBindings({ { outputTextureBinding } });
            vex::BindlessHandle handle = ctx.GetBindlessHandle(outputTextureBinding);

            // Two ray invocations, one for the HLSL shader, and one for the Slang shader.
            ctx.TraceRays(
                { 
                    .rayGenerationShader = 
                    {
                        .path = ExamplesDir / "hello_raytracing" / "HelloRayTracingShader.hlsl",
                        .entryPoint = "RayGenMain",
                        .type = vex::ShaderType::RayGenerationShader,
                    },
                },
                vex::ConstantBinding(handle),
                { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );

#if VEX_SLANG
            ctx.TraceRays(
                { 
                    .rayGenerationShader = 
                    {
                        .path = ExamplesDir / "hello_raytracing" / "HelloRayTracingShader.slang",
                        .entryPoint = "RayGenMain",
                        .type = vex::ShaderType::RayGenerationShader,
                    },
                },
                vex::ConstantBinding(handle),
                { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );
#endif

            // Copy output to the backbuffer.
            ctx.Copy(workingTexture, graphics->GetCurrentPresentTexture());
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void HelloRayTracing::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);

    ExampleApplication::OnResize(window, newWidth, newHeight);

    workingTexture = graphics->CreateTexture({
        .name = "Working Texture",
        .type = vex::TextureType::Texture2D,
        .format = vex::TextureFormat::RGBA8_UNORM,
        .width = newWidth,
        .height = newHeight,
        .depthOrSliceCount = 1,
        .mips = 1,
        .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite,
    });
}

int main()
{
    HelloRayTracing application;
    application.Run();
}