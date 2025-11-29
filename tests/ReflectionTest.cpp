#include "VexTest.h"

#include <gtest/gtest.h>

namespace vex
{

struct ReflectionTest : VexTestParam<ShaderCompilerBackend>
{
};

TEST_P(ReflectionTest, VertexInputLayoutTest)
{
    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);

    struct Vertex
    {
        float position[3];
        float normal[3];
        float normal2[3];
        float uv[2];
        float uv2[2];
    };

    Buffer vbo = graphics.CreateBuffer(BufferDesc::CreateVertexBufferDesc("VBO", sizeof(Vertex) * 10));
    Buffer ibo = graphics.CreateBuffer(BufferDesc::CreateIndexBufferDesc("IBO", sizeof(u16) * 10));
    Texture renderTexture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("RenderTexture",
                                                                                    TextureFormat::RGBA8_UNORM,
                                                                                    100,
                                                                                    100,
                                                                                    1,
                                                                                    TextureUsage::RenderTarget));

    TextureClearValue clearValue{ .flags = TextureClear::ClearColor, .color = { 0.2f, 0.2f, 0.2f, 1 } };
    ctx.ClearTexture(
        TextureBinding{
            .texture = renderTexture,
        },
        clearValue);

    ctx.SetScissor(0, 0, 1, 1);
    ctx.SetViewport(0, 0, 1, 1);

    VertexInputLayout vertexLayout{
        .attributes = {
            {
                .semanticName = "POSITION",
                .semanticIndex = 0,
                .binding = 0,
                .format = TextureFormat::RGB32_FLOAT,
                .offset = 0,
            },
            {
                .semanticName = "TEXCOORD",
                .semanticIndex = 0,
                .binding = 0,
                .format = TextureFormat::RG32_FLOAT,
                .offset = sizeof(float) * 3,
            }
        },
        .bindings = {
            {
                .binding = 0,
                .strideByteSize = static_cast<u32>(sizeof(Vertex)),
                .inputRate = VertexInputLayout::InputRate::PerVertex,
            },
        },
    };

    DepthStencilState depthStencilState{
        .depthTestEnabled = true,
        .depthWriteEnabled = true,
        .depthCompareOp = CompareOp::GreaterEqual,
    };

    std::string shaderExtension = GetParam() == ShaderCompilerBackend::DXC ? "hlsl" : "slang";

    // Setup our draw call's description...
    DrawDesc hlslDrawDesc{
        .vertexShader = { .path = std::format("{}/tests/shaders/VertexInputLayoutTest.{}", VexRootPath.string(), shaderExtension),
                          .entryPoint = "VSMain",
                          .type = ShaderType::VertexShader, },
        .pixelShader = { .path = std::format("{}/tests/shaders/VertexInputLayoutTest.{}", VexRootPath.string(), shaderExtension),
                         .entryPoint = "PSMain",
                         .type = ShaderType::PixelShader, },
        .vertexInputLayout = vertexLayout,
        .depthStencilState = depthStencilState,
    };

    // ...and resources.
    BufferBinding vertexBufferBinding{
        .buffer = vbo,
        .strideByteSize = static_cast<u32>(sizeof(Vertex)),
    };
    BufferBinding indexBufferBinding{
        .buffer = ibo,
        .strideByteSize = static_cast<u32>(sizeof(u32)),
    };

    std::array renderTargets = { TextureBinding{
        .texture = renderTexture,
    } };

    ctx.DrawIndexed(hlslDrawDesc,
                    {
                        .renderTargets = renderTargets,
                        .vertexBuffers = { &vertexBufferBinding, 1 },
                        .indexBuffer = indexBufferBinding,
                    },
                    {},
                    1);
}

INSTANTIATE_TEST_SUITE_P(PerShaderCompiler,
                         ReflectionTest,
                         testing::Values(ShaderCompilerBackend::DXC, ShaderCompilerBackend::Slang));

} // namespace vex