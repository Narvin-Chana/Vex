#include "HelloTriangleApplication.h"

#include <GLFWIncludes.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

HelloTriangleApplication::HelloTriangleApplication()
    : ExampleApplication("HelloTriangleApplication")
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
    finalOutputTexture =
        graphics->CreateTexture({ .name = "Final Output Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::UniformBuffer,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                         vex::ResourceLifetime::Static);

    // Example of GPU only buffer
    commBuffer = graphics->CreateBuffer({ .name = "Comm Buffer",
                                          .byteSize = sizeof(float) * 4,
                                          .usage = vex::BufferUsage::ReadWriteBuffer | vex::BufferUsage::GenericBuffer,
                                          .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                        vex::ResourceLifetime::Static);

    {
        vex::CommandContext ctx =
            graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics, vex::SubmissionPolicy::Immediate);

        const std::filesystem::path uvImagePath =
            std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() / "examples" /
            "uv-guide.png";
        int width, height, channels;
        void* imageData = stbi_load(uvImagePath.string().c_str(), &width, &height, &channels, 4);

        std::vector<vex::u8> fullImageData;
        fullImageData.reserve((width * height + (width / 2) * (height / 2)) * channels);
        std::copy_n(static_cast<vex::u8*>(imageData), width * height * channels, std::back_inserter(fullImageData));

        // Checker board pattern for mip 2
        for (int x = 0; x < width / 2; ++x)
        {
            for (int y = 0; y < height / 2; ++y)
            {
                bool evenX = (x / 20) % 2 == 0;
                bool evenY = (y / 20) % 2 == 0;

                fullImageData.push_back(evenX ^ evenY ? 0 : 0xFF);
                fullImageData.push_back(0x00);
                fullImageData.push_back(0x00);
                fullImageData.push_back(0xFF);
            }
        }

        uvGuideTexture =
            graphics->CreateTexture({ .name = "UV Guide",
                                      .type = vex::TextureType::Texture2D,
                                      .width = static_cast<vex::u32>(width),
                                      .height = static_cast<vex::u32>(height),
                                      .depthOrArraySize = 1,
                                      .mips = 2,
                                      .format = vex::TextureFormat::RGBA8_UNORM,
                                      .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                    vex::ResourceLifetime::Static);

        ctx.EnqueueDataUpload(uvGuideTexture, std::span<const vex::u8>{ fullImageData });

        stbi_image_free(imageData);

        ctx.Submit();
    }

    // Wait for uploads to finish before continuing to main loop.
    graphics->FlushGPU();
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
            float color[4] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            // Create the bindings and obtain the bindless handles we need for our compute passes.
            std::array<vex::ResourceBinding, 4> pass1Bindings{
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
                vex::TextureBinding{ .texture = uvGuideTexture, .usage = vex::TextureBindingUsage::ShaderRead },
            };
            std::vector<vex::BindlessHandle> pass1Handles = ctx.GetBindlessHandles(pass1Bindings);

            std::array<vex::ResourceBinding, 4> pass2Bindings{
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
                vex::TextureBinding{ .texture = uvGuideTexture, .usage = vex::TextureBindingUsage::ShaderRead },
            };
            std::vector<vex::BindlessHandle> pass2Handles = ctx.GetBindlessHandles(pass2Bindings);

            ctx.EnqueueDataUpload(colorBuffer, color);

            // Draw the first pass, first with an HLSL shader, then with a Slang shader (if available).
            {
                ctx.TransitionBindings(pass1Bindings);

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

#if VEX_SLANG
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
                                  .width = newWidth,
                                  .height = newHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);
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
