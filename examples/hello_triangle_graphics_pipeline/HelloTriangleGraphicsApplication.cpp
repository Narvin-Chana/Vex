#include "HelloTriangleGraphicsApplication.h"

#include <span>

#include <GLFWIncludes.h>

HelloTriangleGraphicsApplication::HelloTriangleGraphicsApplication()
    : ExampleApplication("HelloTriangleGraphicsApplication")
{
    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::ShaderReadUniform,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly });

    // Working texture we'll fill in then copy to the backbuffer.
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

void HelloTriangleGraphicsApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Command context is used to record commands to be executed by the GPU.
        vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);
        {
            VEX_GPU_SCOPED_EVENT_COL(ctx, "Triangles", 1, 0, 1);

            // Make the color buffer's contents oscillate over time.
            const double currentTime = glfwGetTime();
            float oscillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOscillatedColor = 1 - oscillatedColor;
            float color[4] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

            // Upload the constant data to our color buffer.
            ctx.EnqueueDataUpload(colorBuffer, std::as_bytes(std::span(color)));
            vex::BufferBinding colorBufferBinding{
                .buffer = colorBuffer,
                .usage = vex::BufferBindingUsage::UniformBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(float) * 4),
            };

            ctx.SetScissor(0, 0, width, height);

            // Clear backbuffer.
            vex::TextureClearValue clearValue{ .color = { 1, 0.5f, 1, 1 } };
            ctx.ClearTexture(graphics->GetCurrentPresentTexture(), clearValue);

            // Setup our draw call's description.
            vex::DrawDesc hlslDrawDesc{
                .vertexShader = shaderCompiler.GetShaderView({
                    .filepath = (ExamplesDir / "hello_triangle_graphics_pipeline" / "HelloTriangleGraphicsShader.hlsl")
                                    .string(),
                    .entryPoint = "VSMain",
                    .type = vex::ShaderType::VertexShader,
                }),
                .pixelShader = shaderCompiler.GetShaderView({
                    .filepath = (ExamplesDir / "hello_triangle_graphics_pipeline" / "HelloTriangleGraphicsShader.hlsl")
                                    .string(),
                    .entryPoint = "PSMain",
                    .type = vex::ShaderType::PixelShader,
                }),
            };

            vex::DrawDesc slangDrawDesc{
                .vertexShader = shaderCompiler.GetShaderView({
                    .filepath = (ExamplesDir / "hello_triangle_graphics_pipeline" / "HelloTriangleGraphicsShader.slang")
                                    .string(),
                    .entryPoint = "VSMain",
                    .type = vex::ShaderType::VertexShader,
                }),
                .pixelShader = shaderCompiler.GetShaderView({
                    .filepath = (ExamplesDir / "hello_triangle_graphics_pipeline" / "HelloTriangleGraphicsShader.slang")
                                    .string(),
                    .entryPoint = "PSMain",
                    .type = vex::ShaderType::PixelShader,
                }),
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
                .colorBufferHandle = graphics->GetBindlessHandle(colorBufferBinding),
                .time = time,
            };

            ctx.SetViewport(0, 0, width / 2.0f, height);
            ctx.Draw(hlslDrawDesc,
                     { .renderTargets = renderTargets },
                     vex::ConstantBinding(lc),
                     { colorBufferBinding },
                     3);
            ctx.SetViewport(width / 2.0f, 0, width / 2.0f, height);
            ctx.Draw(slangDrawDesc,
                     { .renderTargets = renderTargets },
                     vex::ConstantBinding(lc),
                     { colorBufferBinding },
                     3);
        }
        graphics->Submit(ctx);

        graphics->Present();
    }
}

void HelloTriangleGraphicsApplication::OnResize(GLFWwindow* window, int width, int height)
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
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .width = static_cast<uint32_t>(width),
                                  .height = static_cast<uint32_t>(height),
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });
}

int main()
{
    HelloTriangleGraphicsApplication application;
    application.Run();
}