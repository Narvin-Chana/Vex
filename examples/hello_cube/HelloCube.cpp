#include "HelloCube.h"

#include <GLFWIncludes.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct Vertex
{
    float position[3];
    float uv[2];
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

    graphics = CreateGraphicsBackend(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
    SetupShaderErrorHandling();

    // Depth texture
    depthTexture = graphics->CreateTexture({
        .name = "Depth Texture",
        .type = vex::TextureType::Texture2D,
        .format = vex::TextureFormat::D32_FLOAT,
        .width = static_cast<vex::u32>(width),
        .height = static_cast<vex::u32>(height),
        .usage = vex::TextureUsage::DepthStencil,
        .clearValue =
            vex::TextureClearValue{
                .flags = vex::TextureClear::ClearDepth,
                .depth = 0,
            },
    });

    // Vertex buffer
    vertexBuffer =
        graphics->CreateBuffer(vex::BufferDesc::CreateVertexBufferDesc("Vertex Buffer", sizeof(Vertex) * VertexCount));
    // Index buffer
    indexBuffer =
        graphics->CreateBuffer(vex::BufferDesc::CreateIndexBufferDesc("Index Buffer", sizeof(vex::u32) * IndexCount));

    {
        // Immediate submission means the commands are instantly submitted upon destruction.
        vex::CommandContext ctx =
            graphics->BeginScopedCommandContext(vex::QueueType::Graphics, vex::SubmissionPolicy::Immediate);

        // clang-format off

        // Cube vertices (8 vertices for a cube)
        std::array<Vertex, VertexCount> cubeVertices{
            // Front face
            Vertex{ -0.5f, -0.5f, 0.5f, 0, 0 }, // 0: bottom-left
            Vertex{  0.5f, -0.5f, 0.5f, 1, 0 }, // 1: bottom-right
            Vertex{  0.5f,  0.5f, 0.5f, 1, 1 }, // 2: top-right
            Vertex{ -0.5f,  0.5f, 0.5f, 0, 1 }, // 3: top-left
            // Back face
            Vertex{ -0.5f, -0.5f, -0.5f, 1, 0 }, // 4: bottom-left
            Vertex{  0.5f, -0.5f, -0.5f, 0, 0 }, // 5: bottom-right
            Vertex{  0.5f,  0.5f, -0.5f, 0, 1 }, // 6: top-right
            Vertex{ -0.5f,  0.5f, -0.5f, 1, 1 }, // 7: top-left
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

        ctx.EnqueueDataUpload(vertexBuffer, std::as_bytes(std::span(cubeVertices)));
        ctx.EnqueueDataUpload(indexBuffer, std::as_bytes(std::span(cubeIndices)));

        // Use the loaded image for mip index 0.
        const std::filesystem::path uvImagePath = ExamplesDir / "uv-guide.png";
        vex::i32 width, height, channels;
        void* imageData = stbi_load(uvImagePath.string().c_str(), &width, &height, &channels, 4);
        VEX_ASSERT(imageData != nullptr);

        // Vex requires that the upload data for textures be tightly packed together! This shouldn't be an issue as most
        // file formats tightly pack data to avoid wasting space with padding.
        std::vector<vex::u8> fullImageData;
        fullImageData.reserve(width * height * channels);
        std::copy_n(static_cast<vex::u8*>(imageData), width * height * channels, std::back_inserter(fullImageData));

        uvGuideTexture =
            graphics->CreateTexture({ .name = "UV Guide",
                                      .type = vex::TextureType::Texture2D,
                                      .format = vex::TextureFormat::RGBA8_UNORM,
                                      .width = static_cast<vex::u32>(width),
                                      .height = static_cast<vex::u32>(height),
                                      .depthOrSliceCount = 1,
                                      .mips = 0, // 0 means max mips (down to 1x1)
                                      .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });

        // Upload only to the first mip
        ctx.EnqueueDataUpload(
            uvGuideTexture,
            std::as_bytes(std::span(fullImageData.begin(), fullImageData.begin() + width * height * channels)),
            vex::TextureRegion::SingleMip(0));

        // Fill in all mips using the first one.
        ctx.GenerateMips(uvGuideTexture);

        // The texture will now only be used as a read-only shader resource. Avoids having to place a barrier later on.
        // We use PixelShader sync since it will only be used there.
        ctx.Barrier(uvGuideTexture,
                    vex::RHIBarrierSync::PixelShader,
                    vex::RHIBarrierAccess::ShaderRead,
                    vex::RHITextureLayout::ShaderResource);

        stbi_image_free(imageData);
    }

    std::array samplers{
        vex::TextureSampler::CreateSampler(vex::FilterMode::Linear, vex::AddressMode::Clamp),
        vex::TextureSampler::CreateSampler(vex::FilterMode::Point, vex::AddressMode::Clamp),
    };
    graphics->SetSamplers(samplers);
}

void HelloCubeApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        {
            // Make the cube's contents oscillate over time.
            const double currentTime = glfwGetTime();

            // Scoped command context will submit commands automatically upon destruction.
            auto ctx = graphics->BeginScopedCommandContext(vex::QueueType::Graphics);

            ctx.SetScissor(0, 0, width, height);
            ctx.SetViewport(0, 0, width, height);

            // Clear backbuffer.
            vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor,
                                               .color = { 0.2f, 0.2f, 0.2f, 1 } };
            ctx.ClearTexture(
                vex::TextureBinding{
                    .texture = graphics->GetCurrentPresentTexture(),
                },
                clearValue);

            // Clear depth texture.
            ctx.ClearTexture({ .texture = depthTexture });

            vex::VertexInputLayout vertexLayout{
                .attributes = {
                    {
                        .semanticName = "POSITION",
                        .semanticIndex = 0,
                        .binding = 0,
                        .format = vex::TextureFormat::RGB32_FLOAT,
                        .offset = 0,
                    },
                    {
                        .semanticName = "TEXCOORD",
                        .semanticIndex = 0,
                        .binding = 0,
                        .format = vex::TextureFormat::RG32_FLOAT,
                        .offset = sizeof(float) * 3,
                    }
                },
                .bindings = {
                    {
                        .binding = 0,
                        .strideByteSize = static_cast<vex::u32>(sizeof(Vertex)),
                        .inputRate = vex::VertexInputLayout::InputRate::PerVertex,
                    },
                },
            };

            vex::DepthStencilState depthStencilState{
                .depthTestEnabled = true,
                .depthWriteEnabled = true,
                .depthCompareOp = vex::CompareOp::GreaterEqual,
            };

            // Setup our draw call's description...
            vex::DrawDesc hlslDrawDesc{
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
            vex::DrawDesc slangDrawDesc{
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
                .texture = graphics->GetCurrentPresentTexture(),
            } };

            // Usually you'd have to transition the uvGuideTexture (since we're using it bindless-ly), but since we
            // already transitioned it to RHITextureState::ShaderResource after the texture upload we don't have to!
            vex::BindlessHandle uvGuideHandle = ctx.GetBindlessHandle(
                vex::TextureBinding{ .texture = uvGuideTexture, .usage = vex::TextureBindingUsage::ShaderRead });

            struct UniformData
            {
                float currentTime;
                vex::BindlessHandle uvGuideHandle;
            };

            {
                VEX_GPU_SCOPED_EVENT(ctx, "HLSL Cube");
                ctx.DrawIndexed(hlslDrawDesc,
                                {
                                    .renderTargets = renderTargets,
                                    .depthStencil = vex::TextureBinding(depthTexture),
                                    .vertexBuffers = { &vertexBufferBinding, 1 },
                                    .indexBuffer = indexBufferBinding,
                                },
                                vex::ConstantBinding(UniformData{ static_cast<float>(currentTime), uvGuideHandle }),
                                IndexCount);
            }

#if VEX_SLANG
            {
                VEX_GPU_SCOPED_EVENT(ctx, "Slang Cube");
                ctx.DrawIndexed(slangDrawDesc,
                                {
                                    .renderTargets = renderTargets,
                                    .depthStencil = vex::TextureBinding(depthTexture),
                                    .vertexBuffers = { &vertexBufferBinding, 1 },
                                    .indexBuffer = indexBufferBinding,
                                },
                                vex::ConstantBinding(UniformData{ static_cast<float>(currentTime), uvGuideHandle }),
                                IndexCount);
            }
#endif
        }

        graphics->Present(windowMode == Fullscreen);
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

    depthTexture = graphics->CreateTexture({
        .name = "Depth Texture",
        .type = vex::TextureType::Texture2D,
        .format = vex::TextureFormat::D32_FLOAT,
        .width = static_cast<vex::u32>(width),
        .height = static_cast<vex::u32>(height),
        .usage = vex::TextureUsage::DepthStencil,
        .clearValue =
            vex::TextureClearValue{
                .flags = vex::TextureClear::ClearDepth,
                .depth = 0,
            },
    });
}
