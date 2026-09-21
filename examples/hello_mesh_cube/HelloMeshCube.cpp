#include "HelloMeshCube.h"

#include <span>

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

static const vex::ShaderKey HLSLAmplificationShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.hlsl").string(),
    .entryPoint = "ASMain",
    .type = vex::ShaderType::AmplificationShader,
};
static const vex::ShaderKey HLSLMeshShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.hlsl").string(),
    .entryPoint = "MSMain",
    .type = vex::ShaderType::MeshShader,
};
static const vex::ShaderKey HLSLPixelShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.hlsl").string(),
    .entryPoint = "PSMain",
    .type = vex::ShaderType::PixelShader,
};

static const vex::ShaderKey SlangAmplificationShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.slang").string(),
    .entryPoint = "ASMain",
    .type = vex::ShaderType::AmplificationShader,
};
static const vex::ShaderKey SlangMeshShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.slang").string(),
    .entryPoint = "MSMain",
    .type = vex::ShaderType::MeshShader,
};
static const vex::ShaderKey SlangPixelShaderKey{
    .filepath = (ExamplesDir / "hello_mesh_cube" / "HelloCubeShader.slang").string(),
    .entryPoint = "PSMain",
    .type = vex::ShaderType::PixelShader,
};

HelloCubeApplication::HelloCubeApplication()
    : ExampleApplication("HelloCubeApplication")
{
    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
    });

    graphics->SetStaticSamplers(
        { vex::StaticTextureSampler::CreateSampler(vex::FilterMode::Linear, vex::AddressMode::Clamp) });

    // Depth texture
    depthTexture = graphics->CreateTexture({
        .name = "Depth Texture",
        .type = vex::TextureType::Texture2D,
        .format = vex::TextureFormat::D32_FLOAT,
        .width = static_cast<vex::u32>(width),
        .height = static_cast<vex::u32>(height),
        .usage = vex::TextureUsage::DepthStencil,
        .clearValue = vex::TextureClearValue{ .depth = 0 },
    });

    // Vertex buffer
    vertexBuffer =
        graphics->CreateBuffer(vex::BufferDesc::CreateVertexBufferDesc("Vertex Buffer", sizeof(Vertex) * VertexCount));
    // Index buffer
    indexBuffer = graphics->CreateBuffer(
        vex::BufferDesc::CreateIndexBufferDesc("Index Buffer", sizeof(vex::u32) * IndexCount, true));

    {
        // Immediate submission means the commands are instantly submitted upon destruction.
        vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

        // clang-format off

        // Cube vertices (8 vertices for a cube)
        std::array cubeVertices{
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

        graphics->Submit(ctx);
    }
}

void HelloCubeApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        {
            // Make the cube spin over time.
            const double currentTime = glfwGetTime();

            // Scoped command context will submit commands automatically upon destruction.
            vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

            ctx.SetScissor(0, 0, width, height);
            ctx.SetViewport(0, 0, static_cast<float>(width), static_cast<float>(height));

            // Clear present texture.
            vex::TextureClearValue clearValue{ .color = { 0.2f, 0.2f, 0.2f, 1 } };
            ctx.ClearTexture(graphics->GetCurrentPresentTexture(), clearValue);

            // Clear depth texture.
            ctx.ClearTexture(depthTexture);

            vex::DepthStencilState depthStencilState{
                .depthTestEnabled = true,
                .depthWriteEnabled = true,
                .depthCompareOp = vex::CompareOp::GreaterEqual,
            };

            // Setup our draw call's description...
            vex::DispatchMeshDesc hlslDrawDesc{
                .pixelShader = shaderCompiler.GetShaderView(HLSLPixelShaderKey),
                .depthStencilState = depthStencilState,
            };
            hlslDrawDesc.amplificationShader = shaderCompiler.GetShaderView(HLSLAmplificationShaderKey);
            hlslDrawDesc.meshShader = shaderCompiler.GetShaderView(HLSLMeshShaderKey);

            vex::DispatchMeshDesc slangDrawDesc{
                .pixelShader = shaderCompiler.GetShaderView(SlangPixelShaderKey),
                .depthStencilState = depthStencilState,
            };
            slangDrawDesc.amplificationShader = shaderCompiler.GetShaderView(SlangAmplificationShaderKey);
            slangDrawDesc.meshShader = shaderCompiler.GetShaderView(SlangMeshShaderKey);

            // ...and resources.
            vex::BufferBinding indexBufferBinding{
                .buffer = indexBuffer,
                .strideByteSize = static_cast<vex::u32>(sizeof(vex::u32)),
            };

            // Setup our rendering pass.
            std::array renderTargets = { vex::TextureBinding{
                .texture = graphics->GetCurrentPresentTexture(),
            } };

            vex::BindlessHandle vertexBufferHandle =
                graphics->GetBindlessHandle(vex::BufferBinding::CreateStructuredBuffer(vertexBuffer, sizeof(Vertex)));
            vex::BindlessHandle indexBufferHandle =
                graphics->GetBindlessHandle(vex::BufferBinding::CreateStructuredBuffer(indexBuffer, sizeof(vex::u32)));

            struct UniformData
            {
                vex::BindlessHandle vertexBufferHandle;
                vex::BindlessHandle indexBufferHandle;
                float currentTime{};
            };
            UniformData data{
                .vertexBufferHandle = vertexBufferHandle,
                .indexBufferHandle = indexBufferHandle,
                .currentTime = static_cast<float>(currentTime),
            };
            {
                VEX_GPU_SCOPED_EVENT(ctx, "HLSL Cube");
                ctx.DispatchMesh(hlslDrawDesc,
                                 {
                                     .renderTargets = renderTargets,
                                     .depthStencil = vex::TextureBinding(depthTexture),
                                 },
                                 { 2, 1, 1 },
                                 vex::ConstantBinding(data),
                                 {});
            }
            {
                VEX_GPU_SCOPED_EVENT(ctx, "Slang Cube");
                ctx.DispatchMesh(slangDrawDesc,
                                 {
                                     .renderTargets = renderTargets,
                                     .depthStencil = vex::TextureBinding(depthTexture),
                                 },
                                 { 2, 1, 1 },
                                 vex::ConstantBinding(data),
                                 {});
            }
            graphics->Submit(ctx);
        }
        graphics->Present();
    }
}

void HelloCubeApplication::OnResize(GLFWwindow* window, int width, int height)
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
        .clearValue = vex::TextureClearValue{ .depth = 0 },
    });
}

int main()
{
    HelloCubeApplication application;
    application.Run();
}