#include "HelloCube.h"

#include <GLFWIncludes.h>

struct Vertex
{
    float position[3];
};

static constexpr vex::u32 VertexCount = 8;
static constexpr vex::u32 IndexCount = 36;

HelloCubeApplication::HelloCubeApplication()
    : ExampleApplication("HelloCubeApplication")
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

    // Depth texture
    depthTexture = graphics->CreateTexture(
        {
            .name = "Depth Texture",
            .type = vex::TextureType::Texture2D,
            .width = static_cast<vex::u32>(width),
            .height = static_cast<vex::u32>(height),
            .format = vex::TextureFormat::D32_FLOAT,
            .usage = vex::TextureUsage::DepthStencil,
            .clearValue =
                vex::TextureClearValue{
                    .flags = vex::TextureClear::ClearDepth,
                    .depth = 0,
                },
        },
        vex::ResourceLifetime::Static);

    // Vertex buffer
    vertexBuffer = graphics->CreateBuffer({ .name = "Vertex Buffer",
                                            .byteSize = sizeof(Vertex) * VertexCount,
                                            .usage = vex::BufferUsage::VertexBuffer,
                                            .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                          vex::ResourceLifetime::Static);
    // Index buffer
    indexBuffer = graphics->CreateBuffer({ .name = "Index Buffer",
                                           .byteSize = sizeof(vex::u32) * IndexCount,
                                           .usage = vex::BufferUsage::IndexBuffer,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                         vex::ResourceLifetime::Static);

    // Tmp since right now we can't perform work without a present...
    graphics->StartFrame();

    {
        // Scoped command context will submit commands automatically upon destruction.
        auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

        // clang-format off

        // Cube vertices (8 vertices for a cube)
        std::array<Vertex, VertexCount> cubeVertices{
            // Front face
            Vertex{ -0.5f, -0.5f, 0.5f }, // 0: bottom-left
            Vertex{  0.5f, -0.5f, 0.5f }, // 1: bottom-right
            Vertex{  0.5f,  0.5f, 0.5f }, // 2: top-right
            Vertex{ -0.5f,  0.5f, 0.5f }, // 3: top-left
            // Back face
            Vertex{ -0.5f, -0.5f, -0.5f }, // 4: bottom-left
            Vertex{  0.5f, -0.5f, -0.5f }, // 5: bottom-right
            Vertex{  0.5f,  0.5f, -0.5f }, // 6: top-right
            Vertex{ -0.5f,  0.5f, -0.5f }, // 7: top-left
        };
    
        // Cube indices (36 indices for 12 triangles, 2 triangles per face)
        std::array<vex::u32, IndexCount> cubeIndices{
            // Front face
            0, 1, 2,    2, 3, 0,
            // Back face  
            4, 6, 5,    6, 4, 7,
            // Left face
            4, 0, 3,    3, 7, 4,
            // Right face
            1, 5, 6,    6, 2, 1,
            // Top face
            3, 2, 6,    6, 7, 3,
            // Bottom face
            4, 5, 1,    1, 0, 4
        };

        // clang-format on

        ctx.EnqueueDataUpload(vertexBuffer, cubeVertices);
        ctx.EnqueueDataUpload(indexBuffer, cubeIndices);
    }

    // Tmp since right now we can't perform work without a present...
    graphics->EndFrame(windowMode == Fullscreen);

    graphics->FlushGPU();
}

void HelloCubeApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->StartFrame();

        {
            // Make the cube's contents oscillate over time.
            const double currentTime = glfwGetTime();

            // Scoped command context will submit commands automatically upon destruction.
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            ctx.SetScissor(0, 0, width, height);
            ctx.SetViewport(0, 0, width, height);

            // Clear backbuffer.
            vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor,
                                               .color = { 0.2f, 0.2f, 0.2f, 1 } };
            ctx.ClearTexture(
                vex::TextureBinding{
                    .texture = graphics->GetCurrentBackBuffer(),
                },
                clearValue);

            // Clear depth texture.
            ctx.ClearTexture({ .texture = depthTexture });

            vex::VertexInputLayout vertexLayout{
                .attributes = { {
                    .semanticName = "POSITION",
                    .semanticIndex = 0,
                    .binding = 0,
                    .format = vex::TextureFormat::RGB32_FLOAT,
                    .offset = 0,
                } },
                .bindings = { {
                    .binding = 0,
                    .strideByteSize = static_cast<vex::u32>(sizeof(Vertex)),
                    .inputRate = vex::VertexInputLayout::InputRate::PerVertex,
                } },
            };

            vex::DepthStencilState depthStencilState{
                .depthTestEnabled = true,
                .depthWriteEnabled = true,
                .depthCompareOp = vex::CompareOp::GreaterEqual,
            };

            // Setup our draw call's description...
            vex::DrawDescription hlslDrawDesc{
                .vertexShader = { .path = ExamplesDir / "hello_cube" /
                                          "HelloCubeShader.hlsl",
                                  .entryPoint = "VSMain",
                                  .type = vex::ShaderType::VertexShader, },
                .pixelShader = { .path = ExamplesDir / "hello_cube" /
                                         "HelloCubeShader.hlsl",
                                 .entryPoint = "PSMain",
                                 .type = vex::ShaderType::PixelShader, },
                .vertexInputLayout = vertexLayout,
                .depthStencilState = depthStencilState,
            };
#if VEX_SLANG
            vex::DrawDescription slangDrawDesc{
                .vertexShader = { .path = ExamplesDir / "hello_cube" /
                                          "HelloCubeShader.slang",
                                  .entryPoint = "VSMain",
                                  .type = vex::ShaderType::VertexShader, },
                .pixelShader = { .path = ExamplesDir / "hello_cube" /
                                         "HelloCubeShader.slang",
                                 .entryPoint = "PSMain",
                                 .type = vex::ShaderType::PixelShader, },
                .vertexInputLayout = vertexLayout,
                .depthStencilState = depthStencilState,
            };
#endif
            // ...and resources.
            vex::BufferBinding vertexBufferBinding{
                .buffer = vertexBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(Vertex)),
            };
            vex::BufferBinding indexBufferBinding{
                .buffer = indexBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(vex::u32)),
            };

            // Setup our rendering pass.
            std::array renderTargets = { vex::TextureBinding{
                .texture = graphics->GetCurrentBackBuffer(),
            } };

            ctx.DrawIndexed(hlslDrawDesc,
                            {
                                .renderTargets = renderTargets,
                                .depthStencil = vex::TextureBinding(depthTexture),
                                .vertexBuffers = { &vertexBufferBinding, 1 },
                                .indexBuffer = indexBufferBinding,
                            },
                            vex::ConstantBinding(static_cast<float>(currentTime)),
                            IndexCount);
#if VEX_SLANG
            ctx.DrawIndexed(slangDrawDesc,
                            {
                                .renderTargets = renderTargets,
                                .depthStencil = vex::TextureBinding(depthTexture),
                                .vertexBuffers = { &vertexBufferBinding, 1 },
                                .indexBuffer = indexBufferBinding,
                            },
                            vex::ConstantBinding(static_cast<float>(currentTime)),
                            IndexCount);
#endif
        }

        graphics->EndFrame(windowMode == Fullscreen);
    }
}

void HelloCubeApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    graphics->DestroyTexture(depthTexture);

    ExampleApplication::OnResize(window, width, height);

    depthTexture = graphics->CreateTexture(
        {
            .name = "Depth Texture",
            .type = vex::TextureType::Texture2D,
            .width = static_cast<vex::u32>(width),
            .height = static_cast<vex::u32>(height),
            .format = vex::TextureFormat::D32_FLOAT,
            .usage = vex::TextureUsage::DepthStencil,
            .clearValue =
                vex::TextureClearValue{
                    .flags = vex::TextureClear::ClearDepth,
                    .depth = 0,
                },
        },
        vex::ResourceLifetime::Static);
}
