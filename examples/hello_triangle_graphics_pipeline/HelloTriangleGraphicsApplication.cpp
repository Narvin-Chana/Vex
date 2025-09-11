#include "HelloTriangleGraphicsApplication.h"

#include <GLFWIncludes.h>

HelloTriangleGraphicsApplication::HelloTriangleGraphicsApplication()
    : ExampleApplication("HelloTriangleGraphicsApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = CreateGraphicsBackend(vex::BackendDescription{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
    SetupShaderErrorHandling();

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::UniformBuffer,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                         vex::ResourceLifetime::Static);

    // Working texture we'll fill in then copy to the backbuffer.
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

void HelloTriangleGraphicsApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        {
            // Make the color buffer's contents oscillate over time.
            const double currentTime = glfwGetTime();
            float oscillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOscillatedColor = 1 - oscillatedColor;
            float color[4] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

            // Scoped command context will submit commands automatically upon destruction.
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            ctx.EnqueueDataUpload(colorBuffer, std::as_bytes(std::span(color)));

            ctx.SetScissor(0, 0, width, height);

            // Clear backbuffer.
            vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 1, 0.5f, 1, 1 } };
            ctx.ClearTexture(
                vex::TextureBinding{
                    .texture = graphics->GetCurrentPresentTexture(),
                },
                clearValue);

            // Setup our draw call's description...
            vex::DrawDescription hlslDrawDesc{
                .vertexShader = { .path = ExamplesDir / "hello_triangle_graphics_pipeline" /
                                          "HelloTriangleGraphicsShader.hlsl",
                                  .entryPoint = "VSMain",
                                  .type = vex::ShaderType::VertexShader, },
                .pixelShader = { .path = ExamplesDir / "hello_triangle_graphics_pipeline" /
                                         "HelloTriangleGraphicsShader.hlsl",
                                 .entryPoint = "PSMain",
                                 .type = vex::ShaderType::PixelShader, },
            };
#if VEX_SLANG
            vex::DrawDescription slangDrawDesc{
                .vertexShader = { .path = ExamplesDir / "hello_triangle_graphics_pipeline" /
                                          "HelloTriangleGraphicsShader.slang",
                                  .entryPoint = "VSMain",
                                  .type = vex::ShaderType::VertexShader, },
                .pixelShader = { .path = ExamplesDir / "hello_triangle_graphics_pipeline" /
                                         "HelloTriangleGraphicsShader.slang",
                                 .entryPoint = "PSMain",
                                 .type = vex::ShaderType::PixelShader, },
            };
#endif
            // ...and resources.
            vex::BufferBinding colorBufferBinding{
                .buffer = colorBuffer,
                .usage = vex::BufferBindingUsage::ConstantBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(float) * 4),
            };

            // Setup our rendering pass.
            std::array renderTargets = { vex::TextureBinding{
                .texture = graphics->GetCurrentPresentTexture(),
            } };

            // Cursed float overflow UB greatness.
            static float time = 0;
            time += static_cast<float>(currentTime / 1000.0);

            struct LocalConstants
            {
                vex::BindlessHandle colorBufferHandle;
                float time;
            };

            LocalConstants lc{
                .colorBufferHandle = ctx.GetBindlessHandle(colorBufferBinding),
                .time = time,
            };

            ctx.TransitionBindings({ { colorBufferBinding } });

            ctx.SetViewport(0, 0, width / 2.0f, height);
            ctx.Draw(hlslDrawDesc, { .renderTargets = renderTargets }, vex::ConstantBinding(lc), 3);
            ctx.SetViewport(width / 2.0f, 0, width / 2.0f, height);
            ctx.Draw(
#if VEX_SLANG
                slangDrawDesc,
#else
                hlslDrawDesc,
#endif
                { .renderTargets = renderTargets },
                vex::ConstantBinding(lc),
                3);
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void HelloTriangleGraphicsApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);

    ExampleApplication::OnResize(window, width, height);

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = width,
                                  .height = height,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);
}
