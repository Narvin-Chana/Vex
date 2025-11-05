#include "HelloTriangleApplication.h"

#include <GLFWIncludes.h>

HelloTriangleApplication::HelloTriangleApplication()
    : ExampleApplication("HelloTriangleApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
        .frameBuffering = vex::FrameBuffering::Triple,
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
    finalOutputTexture =
        graphics->CreateTexture({ .name = "Final Output Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::UniformBuffer,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly });

    // Example of GPU only buffer
    commBuffer = graphics->CreateBuffer({ .name = "Comm Buffer",
                                          .byteSize = sizeof(float) * 4,
                                          .usage = vex::BufferUsage::ReadWriteBuffer | vex::BufferUsage::GenericBuffer,
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

            auto ctx = graphics->BeginScopedCommandContext(vex::QueueType::Graphics);

            // Create the bindings and obtain the bindless handles we need for our compute passes.
            std::array<vex::ResourceBinding, 3> pass1Bindings{
                vex::BufferBinding{
                    .buffer = colorBuffer,
                    .usage = vex::BufferBindingUsage::ConstantBuffer,
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
            std::vector<vex::BindlessHandle> pass1Handles = ctx.GetBindlessHandles(pass1Bindings);

            std::array<vex::ResourceBinding, 3> pass2Bindings{
                vex::TextureBinding{
                    .texture = finalOutputTexture,
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
            std::vector<vex::BindlessHandle> pass2Handles = ctx.GetBindlessHandles(pass2Bindings);

            ctx.EnqueueDataUpload(colorBuffer, std::as_bytes(std::span(color)));

            // Draw the first pass, first with an HLSL shader, then with a Slang shader (if available).
            {
                ctx.TransitionBindings(pass1Bindings);

                {
                    VEX_GPU_SCOPED_EVENT_COL(ctx, "HLSL Triangle", 1, 0, 1)
                    ctx.Dispatch(
                        {
                            .path = ExamplesDir / "hello_triangle" / "HelloTriangleShader.cs.hlsl",
                            .entryPoint = "CSMain",
                            .type = vex::ShaderType::ComputeShader,
                        },
                        vex::ConstantBinding(std::span(pass1Handles)),
                        {
                            (width + 7u) / 8u,
                            (height + 7u) / 8u,
                            1u,
                        });
                }

#if VEX_SLANG
                {
                    VEX_GPU_SCOPED_EVENT_COL(ctx, "Slang Triangle", 1, 1, 0)
                    ctx.Dispatch(
                        {
                            .path = ExamplesDir / "hello_triangle" / "HelloTriangleShader.slang",
                            .entryPoint = "CSMain",
                            .type = vex::ShaderType::ComputeShader,
                        },
                        vex::ConstantBinding(std::span(pass1Handles)),
                        {
                            (width + 7u) / 8u,
                            (height + 7u) / 8u,
                            1u,
                        });
                }
#endif
            }

            // Draw the second pass, first with an HLSL shader, then with a Slang shader (if available).
            {
                ctx.TransitionBindings(pass2Bindings);

                ctx.Dispatch(
                    {
                        .path = ExamplesDir / "hello_triangle" / "HelloTriangleShader2.cs.hlsl",
                        .entryPoint = "CSMain",
                        .type = vex::ShaderType::ComputeShader,
                    },
                    vex::ConstantBinding(std::span(pass2Handles)),
                    {
                        (width + 7u) / 8u,
                        (height + 7u) / 8u,
                        1u,
                    });

#if VEX_SLANG
                ctx.Dispatch(
                    {
                        .path = ExamplesDir / "hello_triangle" / "HelloTriangleShader2.slang",
                        .entryPoint = "CSMain",
                        .type = vex::ShaderType::ComputeShader,
                    },
                    vex::ConstantBinding(std::span(pass2Handles)),
                    {
                        (width + 7u) / 8u,
                        (height + 7u) / 8u,
                        1u,
                    });
#endif
            }

            ctx.Copy(finalOutputTexture, graphics->GetCurrentPresentTexture());
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void HelloTriangleApplication::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);
    graphics->DestroyTexture(finalOutputTexture);

    ExampleApplication::OnResize(window, newWidth, newHeight);

    finalOutputTexture =
        graphics->CreateTexture({ .name = "Final Output Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .width = newWidth,
                                  .height = newHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });
    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .width = newWidth,
                                  .height = newHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });
}
