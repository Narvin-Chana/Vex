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

    graphics = CreateGraphicsBackend(vex::BackendDescription{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
        .frameBuffering = vex::FrameBuffering::Triple,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    SetupShaderErrorHandling();

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);
}

void HelloRayTracing::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->StartFrame();

        {
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            ctx.TraceRays(
                { 
                    .rayGenerationShader = 
                    {
                        .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
                                "examples" / "hello_raytracing" / "HelloRayTracingShader.hlsl",
                        .entryPoint = "RayGenMain",
                        .type = vex::ShaderType::RayGenerationShader,
                    },
                },
                {{
                    vex::TextureBinding
                    {
                        .name = "OutputTexture",
                        .texture = workingTexture,
                        .usage = vex::TextureBindingUsage::ShaderReadWrite,
                    },
                }},
                {},
                { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );

            // Copy output to the backbuffer.
            ctx.Copy(workingTexture, graphics->GetCurrentBackBuffer());
        }

        graphics->EndFrame(windowMode == Fullscreen);
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

    workingTexture = graphics->CreateTexture(
        {
            .name = "Working Texture",
            .type = vex::TextureType::Texture2D,
            .width = newWidth,
            .height = newHeight,
            .depthOrArraySize = 1,
            .mips = 1,
            .format = vex::TextureFormat::RGBA8_UNORM,
            .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite,
        },
        vex::ResourceLifetime::Static);
}
