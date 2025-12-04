#include "VexTest.h"

#include <gtest/gtest.h>

namespace vex
{

struct ReflectionTestFull : VexPerShaderCompilerTest
{
};

TEST_P(ReflectionTestFull, CompleteGraphicsPSOTest)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

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

    // Setup our draw call's description...
    DrawDesc hlslDrawDesc{
        .vertexShader = { .path = std::format("{}/tests/shaders/VertexInputLayoutTest.{}", VexRootPath.string(), GetShaderExtension(GetShaderCompilerBackend())),
                          .entryPoint = "VSMain",
                          .type = ShaderType::VertexShader, },
        .pixelShader = { .path = std::format("{}/tests/shaders/VertexInputLayoutTest.{}", VexRootPath.string(), GetShaderExtension(GetShaderCompilerBackend())),
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

    graphics.Submit(ctx);
}

INSTANTIATE_TEST_SUITE_P(PerShaderCompilerBackend, ReflectionTestFull, ShaderCompilerBackendValues);

struct VertexShaderReflectionTestParam
{
    std::string_view entryPoint;
    VertexInputLayout inputLayoutToValidate;
};
struct VertexShaderReflectionTest : VexPerShaderCompilerTestParam<VertexShaderReflectionTestParam>
{
};

void ValidateShaderReflection(const Shader& shader, const VertexInputLayout& inputLayout)
{
    const ShaderReflection* reflection = shader.GetReflection();
    if (!reflection)
        return;

    ASSERT_TRUE(reflection->inputs.size() == inputLayout.attributes.size());
    for (u32 i = 0; i < reflection->inputs.size(); ++i)
    {
        ASSERT_TRUE(reflection->inputs[i].semanticName == inputLayout.attributes[i].semanticName);
        ASSERT_TRUE(reflection->inputs[i].semanticIndex == inputLayout.attributes[i].semanticIndex);
        ASSERT_TRUE(reflection->inputs[i].format == inputLayout.attributes[i].format);
    }
}

TEST_P(VertexShaderReflectionTest, VertexShaderReflection)
{
    ShaderCompiler compiler{};

    VertexShaderReflectionTestParam param = GetParam();

    ShaderKey shaderKey{
        .path = std::format("{}/tests/shaders/reflection/Semantics.{}",
                            VexRootPath.string(),
                            GetShaderExtension(GetShaderCompilerBackend())),
        .entryPoint = std::string(param.entryPoint),
        .type = ShaderType::VertexShader,
    };

    Shader shader{ shaderKey };
    auto res = compiler.CompileShader(shader);
    VEX_ASSERT(res.has_value());

    ValidateShaderReflection(shader, param.inputLayoutToValidate);
}

INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(PerShaderCompiler, VertexShaderReflectionTest,
                        testing::Values(
                            VertexShaderReflectionTestParam{
                                .entryPoint = "ReflectionVertex1",
                                .inputLayoutToValidate = {
                                    .attributes = { {
                                                        .semanticName = "POSITION",
                                                        .semanticIndex = 0,
                                                        .format = TextureFormat::RGB32_FLOAT,
                                                    },
                                                    {
                                                        .semanticName = "TEXCOORD",
                                                        .semanticIndex = 0,
                                                        .format = TextureFormat::RG32_FLOAT,
                                                    } },
                                }
                            },
                            VertexShaderReflectionTestParam{
                                .entryPoint = "ReflectionVertex2",
                                .inputLayoutToValidate = {
                                    .attributes = { {
                                                        .semanticName = "POSITION",
                                                        .semanticIndex = 0,
                                                        .format = TextureFormat::RGB32_FLOAT,
                                                    },
                                                    {
                                                        .semanticName = "TEXCOORD",
                                                        .semanticIndex = 0,
                                                        .format = TextureFormat::RG32_FLOAT,
                                                    } },
                                }
                            },
                            VertexShaderReflectionTestParam{
                                .entryPoint = "ReflectionVertex3",
                                .inputLayoutToValidate = {
                                    .attributes = { {
                                                        .semanticName = "POSITION",
                                                        .semanticIndex = 0,
                                                        .format = TextureFormat::RGB32_FLOAT,
                                                    },
                                                    {
                                                        .semanticName = "POSITION",
                                                        .semanticIndex = 1,
                                                        .format = TextureFormat::RGB32_FLOAT,
                                                    },
                                                    {
                                                        .semanticName = "TEXCOORD",
                                                        .semanticIndex = 1,
                                                        .format = TextureFormat::RG32_FLOAT,
                                                    } },
                                }
                            }
                        ));

struct ComputeShaderReflectionTestParam
{
    std::string_view entryPoint;
    ShaderReflection expectedReflection;
};
struct ComputeShaderReflectionTest : VexPerShaderCompilerTestParam<ComputeShaderReflectionTestParam>
{
};

TEST_P(ComputeShaderReflectionTest, ComputShaderReflection)
{
    ShaderCompiler compiler{};

    ComputeShaderReflectionTestParam param = GetParam();

    ShaderKey shaderKey{
        .path = std::format("{}/tests/shaders/reflection/Semantics.{}",
                            VexRootPath.string(),
                            GetShaderExtension(GetShaderCompilerBackend())),
        .entryPoint = std::string(param.entryPoint),
        .type = ShaderType::ComputeShader,
    };

    Shader shader{ shaderKey };
    auto res = compiler.CompileShader(shader);
    VEX_ASSERT(res.has_value());

    ASSERT_TRUE(shader.GetReflection() && (*shader.GetReflection() == param.expectedReflection));
}

INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(
    PerShaderCompiler,
    ComputeShaderReflectionTest,
    testing::Values(ComputeShaderReflectionTestParam{ "ReflectionCompute1", {} },
                    ComputeShaderReflectionTestParam{ "ReflectionCompute2", {} },
                    ComputeShaderReflectionTestParam{ "ReflectionCompute3", {} }));

struct PixelShaderReflectionTestParam
{
    std::string_view entryPoint;
    ShaderReflection expectedReflection;
};
struct PixelShaderReflectionTest : VexPerShaderCompilerTestParam<PixelShaderReflectionTestParam>
{
};

TEST_P(PixelShaderReflectionTest, PixelShaderReflection)
{
    ShaderCompiler compiler{};

    PixelShaderReflectionTestParam param = GetParam();

    ShaderKey shaderKey{
        .path = std::format("{}/tests/shaders/reflection/Semantics.{}",
                            VexRootPath.string(),
                            GetShaderExtension(GetShaderCompilerBackend())),
        .entryPoint = std::string(param.entryPoint),
        .type = ShaderType::PixelShader,
    };

    Shader shader{ shaderKey };
    auto res = compiler.CompileShader(shader);
    VEX_ASSERT(res.has_value());

    ASSERT_TRUE(shader.GetReflection() && (*shader.GetReflection() == param.expectedReflection));
}

INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(PerShaderCompiler,
                                             PixelShaderReflectionTest,
                                             testing::Values(PixelShaderReflectionTestParam{ "ReflectionPixel1", {} }));

} // namespace vex