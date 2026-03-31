#include "HelloTriangleApplication.h"

#include <GLFWIncludes.h>

HelloTriangleApplication::HelloTriangleApplication()
    : ExampleApplication("HelloTriangleApplication")
{
    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    vex::TextureFormat presentTextureFormat = graphics->GetCurrentPresentTexture().desc.format;

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = presentTextureFormat,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::ShaderReadUniform,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly });

    // Example of GPU only buffer
    commBuffer = graphics->CreateBuffer({ .name = "Comm Buffer",
                                          .byteSize = sizeof(float) * 4,
                                          .usage = vex::BufferUsage::ShaderReadWrite | vex::BufferUsage::ShaderRead,
                                          .memoryLocality = vex::ResourceMemoryLocality::GPUOnly });
}

void HelloTriangleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const double currentTime = glfwGetTime();

        {
            float oscillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOscillatedColor = 1 - oscillatedColor;
            float color[] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

            // Command context is used to record commands to be executed by the GPU.
            vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

            // Create the bindings and obtain the bindless handles we need for our compute passes.
            std::array<vex::ResourceBinding, 3> pass1Bindings{
                vex::BufferBinding{
                    .buffer = colorBuffer,
                    .usage = vex::BufferBindingUsage::UniformBuffer,
                },
                vex::BufferBinding{
                    .buffer = commBuffer,
                    .usage = vex::BufferBindingUsage::RWStructuredBuffer,
                    .strideByteSize = static_cast<vex::u32>(sizeof(float) * 4),
                },
                vex::TextureBinding{
                    .texture = workingTexture,
                    .usage = vex::TextureBindingUsage::ShaderReadWrite,
                },
            };
            std::vector<vex::BindlessHandle> pass1Handles = graphics->GetBindlessHandles(pass1Bindings);

            std::array<vex::ResourceBinding, 3> pass2Bindings{
                vex::TextureBinding{
                    .texture = graphics->GetCurrentPresentTexture(),
                    .usage = vex::TextureBindingUsage::ShaderReadWrite,
                },
                vex::BufferBinding{
                    .buffer = commBuffer,
                    .usage = vex::BufferBindingUsage::StructuredBuffer,
                    .strideByteSize = static_cast<vex::u32>(sizeof(float) * 4),
                },
                vex::TextureBinding{
                    .texture = workingTexture,
                    .usage = vex::TextureBindingUsage::ShaderRead,
                },
            };
            std::vector<vex::BindlessHandle> pass2Handles = graphics->GetBindlessHandles(pass2Bindings);

            ctx.EnqueueDataUpload(colorBuffer, std::as_bytes(std::span(color)));

            // Draw the first pass, first with an HLSL shader, then with a Slang shader (if available).
            {
                {
                    VEX_GPU_SCOPED_EVENT_COL(ctx, "HLSL Triangle", 1, 0, 1)
                    ctx.Dispatch(
                        shaderCompiler.GetShaderView({
                            .filepath = (ExamplesDir / "hello_triangle" / "HelloTriangleShader.cs.hlsl").string(),
                            .entryPoint = "CSMain",
                            .type = vex::ShaderType::ComputeShader,
                        }),
                        vex::ConstantBinding(std::span(pass1Handles)),
                        { pass1Bindings },
                        {
                            (width + 7u) / 8u,
                            (height + 7u) / 8u,
                            1u,
                        });
                }

                {
                    VEX_GPU_SCOPED_EVENT_COL(ctx, "Slang Triangle", 1, 1, 0)
                    ctx.Dispatch(
                        shaderCompiler.GetShaderView({
                            .filepath = (ExamplesDir / "hello_triangle" / "HelloTriangleShader.slang").string(),
                            .entryPoint = "CSMain",
                            .type = vex::ShaderType::ComputeShader,
                        }),
                        vex::ConstantBinding(std::span(pass1Handles)),
                        { pass1Bindings },
                        {
                            (width + 7u) / 8u,
                            (height + 7u) / 8u,
                            1u,
                        });
                }
            }

            // Draw the second pass, first with an HLSL shader, then with a Slang shader (if available).
            {
                ctx.Dispatch(shaderCompiler.GetShaderView({
                                 .filepath = (ExamplesDir / "hello_triangle" / "HelloTriangleShader2.cs.hlsl").string(),
                                 .entryPoint = "CSMain",
                                 .type = vex::ShaderType::ComputeShader,
                             }),
                             vex::ConstantBinding(std::span(pass2Handles)),
                             { pass2Bindings },
                             {
                                 (width + 7u) / 8u,
                                 (height + 7u) / 8u,
                                 1u,
                             });

                ctx.Dispatch(shaderCompiler.GetShaderView({
                                 .filepath = (ExamplesDir / "hello_triangle" / "HelloTriangleShader2.slang").string(),
                                 .entryPoint = "CSMain",
                                 .type = vex::ShaderType::ComputeShader,
                             }),
                             vex::ConstantBinding(std::span(pass2Handles)),
                             { pass2Bindings },
                             {
                                 (width + 7u) / 8u,
                                 (height + 7u) / 8u,
                                 1u,
                             });
            }

            graphics->Submit(ctx);
        }

        graphics->Present();
    }
}

void HelloTriangleApplication::OnResize(GLFWwindow* window, int newWidth, int newHeight)
{
    if (newWidth == 0 || newHeight == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);

    ExampleApplication::OnResize(window, newWidth, newHeight);

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = graphics->GetCurrentPresentTexture().desc.format,
                                  .width = static_cast<uint32_t>(newWidth),
                                  .height = static_cast<uint32_t>(newHeight),
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });
}

int main()
{
    HelloTriangleApplication application;
    application.Run();
}