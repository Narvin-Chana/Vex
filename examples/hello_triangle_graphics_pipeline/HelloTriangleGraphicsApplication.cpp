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

    std::vector<vex::TextureSampler> samplers{ vex::TextureSampler{ .name = "MySampler" } };
    graphics->SetSamplers(samplers);

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

        graphics->StartFrame();

        // clang-format off

        {
            const double currentTime = glfwGetTime();
            float oscillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOscillatedColor = 1 - oscillatedColor;
            float color[4] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

            // Scoped command context will submit commands automatically upon destruction.
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            graphics->MapResource(ctx, colorBuffer).SetData(color);

            ctx.SetScissor(0, 0, width, height);

            // Clear backbuffer.
            vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 1, 0.5f, 1, 1 } };
            ctx.ClearTexture(vex::TextureBinding{
                .name = "Backbuffer",
                .texture = graphics->GetCurrentBackBuffer(),
            }, clearValue);


            // Setup our rendering pass.
            std::array renderTargets = {
                vex::TextureBinding{
                    .name = "OutputTexture",
                    .texture = graphics->GetCurrentBackBuffer(),
                }
            };

            // Allows for rendering to these render targets!
            ctx.BeginRendering({ .renderTargets = renderTargets, .depthStencil = std::nullopt });

            // Setup our draw call's description...
            static std::filesystem::path shaderFolderPath = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() / "examples" / "hello_triangle_graphics_pipeline";
            vex::DrawDescription drawDesc{
                .vertexShader = {
                    .path = shaderFolderPath / "HelloTriangleGraphicsShader.hlsl",
                    .entryPoint = "VSMain",
                    .type = vex::ShaderType::VertexShader
                },
                .pixelShader = {
                    .path = shaderFolderPath / "HelloTriangleGraphicsShader.hlsl",
                    .entryPoint = "PSMain",
                    .type = vex::ShaderType::PixelShader
                }
            };
            // ...and resources.

            std::array<vex::ResourceBinding, 1> resourceBindings = {
                vex::BufferBinding{
                    .name = "ColorBuffer",
                    .buffer = colorBuffer,
                    .usage = vex::BufferBindingUsage::ConstantBuffer,
                    .stride = sizeof(float) * 4,
                }
            };

            // Cursed float overflow UB greatness.
            static float time = 0;
            time += static_cast<float>(currentTime / 1000.0);

            vex::DrawResources drawResources{
                .constants = time,
                .resourceBindings = resourceBindings,
            };

            ctx.SetViewport(0, 0, width / 2.0f, height);
            ctx.Draw(drawDesc, drawResources, 3);
            ctx.SetViewport(width / 2.0f, 0, width / 2.0f, height);
            ctx.Draw(drawDesc, drawResources, 3);

            // Ends the rendering pass, MUST be called.
            ctx.EndRendering();
        }

        // clang-format on

        graphics->EndFrame(windowMode == Fullscreen);
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
