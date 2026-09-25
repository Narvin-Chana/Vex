#include "VexTest.h"

using namespace vex;

struct GraphicsPipelineTests : VexTest
{
};

TEST_F(GraphicsPipelineTests, DepthOnlyDraw)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                           TextureFormat::D24_UNORM_S8_UINT,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::DepthStencil));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    DrawDesc desc{
        .vertexShader = shaderCompiler.GetShaderView(ShaderKey{
            .filepath = (TestShaderPath / "GraphicsPipeline.hlsl").string(),
            .entryPoint = "VSMain",
            .type = ShaderType::VertexShader
        }),
        .depthStencilState = {
            .depthTestEnabled = true,
            .depthWriteEnabled = true,
        },
    };

    DrawResourceBinding drawRes{ .depthStencil = TextureBinding{ texture } };

    ctx.SetScissor(0, 0, 10, 10);
    ctx.SetViewport(0, 0, 10, 10);
    ctx.ClearTexture(texture, TextureClearValue{ .depth = 1 });
    ctx.Draw(desc, drawRes, {}, {}, 3);

    TextureReadbackContext readbackColorCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Depth));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(!ValidateTextureValue(readbackColorCtx, std::array<u8, 4>{ 1, 1, 1, 1 }));
}