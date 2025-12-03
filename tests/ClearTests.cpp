#include "VexTest.h"

namespace vex
{
struct ClearTest : VexTest
{
};

bool ValidateTextureValue(const TextureReadbackContext& ctx, u32 expectedColor, u32 mask = 0xFFFFFFFF)
{
    std::vector<u32> texels;
    texels.resize(ctx.GetDataByteSize() / 4);
    ctx.ReadData(std::as_writable_bytes(std::span{ texels }));
    return std::ranges::all_of(texels, [&](auto v) { return (v & mask) == expectedColor; });
}

template <class F>
TextureReadbackContext ExecuteAndReadback(Graphics& gfx, const Texture& texture, F func)
{
    auto ctx = gfx.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    func(ctx, texture);
    auto readbackCtx = ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0));
    gfx.WaitForTokenOnCPU(ctx.Submit());
    return readbackCtx;
}

TEST_F(ClearTest, ClearRenderTargetImplicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc(
        "TestRenderTarget",
        TextureFormat::BGRA8_UNORM,
        10,
        10,
        1,
        TextureUsage::RenderTarget,
        TextureClearValue{ .flags = TextureClear::ClearColor, .color = { 1, 1, 1, 1 } }));

    auto readbackCtx = ExecuteAndReadback(graphics,
                                          texture,
                                          [](CommandContext& ctx, const Texture& texture)
                                          { ctx.ClearTexture({ .texture = texture }); });

    EXPECT_TRUE(ValidateTextureValue(readbackCtx, 0xFFFFFFFF));
}

TEST_F(ClearTest, ClearRenderTargetExplicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestRenderTarget",
                                                                           TextureFormat::BGRA8_UNORM,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::RenderTarget));

    auto readbackCtx = ExecuteAndReadback(
        graphics,
        texture,
        [](CommandContext& ctx, const Texture& texture)
        {
            ctx.ClearTexture({ .texture = texture },
                             TextureClearValue{ .flags = TextureClear::ClearColor, .color = { 1, 1, 1, 1 } });
        });

    EXPECT_TRUE(ValidateTextureValue(readbackCtx, 0xFFFFFFFF));
}

TEST_F(ClearTest, ClearDepthOnlyImplicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                           TextureFormat::D32_FLOAT,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::DepthStencil,
                                                                           TextureClearValue{
                                                                               .flags = TextureClear::ClearDepth,
                                                                               .depth = 0.54f,
                                                                           }));

    auto depthReadback = ExecuteAndReadback(graphics,
                                            texture,
                                            [](CommandContext& ctx, const Texture& texture)
                                            { ctx.ClearTexture({ .texture = texture }); });

    float value = 0.54f;
    EXPECT_TRUE(ValidateTextureValue(depthReadback, reinterpret_cast<u32&>(value)));
}

TEST_F(ClearTest, ClearDepthOnlyExpicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                           TextureFormat::D32_FLOAT,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::DepthStencil));

    auto depthReadback = ExecuteAndReadback(graphics,
                                            texture,
                                            [](CommandContext& ctx, const Texture& texture)
                                            {
                                                ctx.ClearTexture({ .texture = texture },
                                                                 TextureClearValue{
                                                                     .flags = TextureClear::ClearDepth,
                                                                     .depth = 0.54f,
                                                                 });
                                            });

    float value = 0.54f;
    EXPECT_TRUE(ValidateTextureValue(depthReadback, reinterpret_cast<u32&>(value)));
}

// TODO: Uncomment those tests when https://trello.com/c/vEaa2SUe is done
//
// TEST_F(ClearTest, ClearDepthStencilImplicit)
// {
//     auto texture = graphics.CreateTexture(
//         TextureDesc::CreateTexture2DDesc("TestDepthStencil",
//                                          TextureFormat::D24_UNORM_S8_UINT,
//                                          10,
//                                          10,
//                                          1,
//                                          TextureUsage::DepthStencil,
//                                          TextureClearValue{
//                                              .flags = TextureClear::ClearStencil | TextureClear::ClearDepth,
//                                              .depth = 1.0f,
//                                              .stencil = 0xEE,
//                                          }));
//
//     auto depthReadback = ExecuteAndReadback(graphics,
//                                             texture,
//                                             [](CommandContext& ctx, const Texture& texture)
//                                             { ctx.ClearTexture({ .texture = texture }); });
//
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0x00FFFFFF, 0x00FFFFFF));
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0xEE000000, 0xFF000000));
// }
//
// TEST_F(ClearTest, ClearDepthStencilExplicit)
// {
//     auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
//                                                                            TextureFormat::D24_UNORM_S8_UINT,
//                                                                            10,
//                                                                            10,
//                                                                            1,
//                                                                            TextureUsage::DepthStencil));
//
//     auto depthReadback =
//         ExecuteAndReadback(graphics,
//                            texture,
//                            [](CommandContext& ctx, const Texture& texture)
//                            {
//                                ctx.ClearTexture({ .texture = texture },
//                                                 TextureClearValue{
//                                                     .flags = TextureClear::ClearStencil | TextureClear::ClearDepth,
//                                                     .depth = 1.0f,
//                                                     .stencil = 0xEE,
//                                                 });
//                            });
//
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0x00FFFFFF, 0x00FFFFFF));
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0xEE000000, 0xFF000000));
// }
//
// TEST_F(ClearTest, ClearStencilImplicit)
// {
//     auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
//                                                                            TextureFormat::D24_UNORM_S8_UINT,
//                                                                            10,
//                                                                            10,
//                                                                            1,
//                                                                            TextureUsage::DepthStencil,
//                                                                            TextureClearValue{
//                                                                                .flags = TextureClear::ClearStencil,
//                                                                                .stencil = 0xEE,
//                                                                            }));
//
//     auto depthReadback = ExecuteAndReadback(graphics,
//                                             texture,
//                                             [](CommandContext& ctx, const Texture& texture)
//                                             { ctx.ClearTexture({ .texture = texture }); });
//
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0xEE000000, 0xFF000000));
// }
//
// TEST_F(ClearTest, ClearStencilExplicit)
// {
//     auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
//                                                                            TextureFormat::D24_UNORM_S8_UINT,
//                                                                            10,
//                                                                            10,
//                                                                            1,
//                                                                            TextureUsage::DepthStencil));
//
//     auto depthReadback = ExecuteAndReadback(graphics,
//                                             texture,
//                                             [](CommandContext& ctx, const Texture& texture)
//                                             {
//                                                 ctx.ClearTexture({ .texture = texture },
//                                                                  TextureClearValue{
//                                                                      .flags = TextureClear::ClearStencil,
//                                                                      .stencil = 0xEE,
//                                                                  });
//                                             });
//
//     EXPECT_TRUE(ValidateTextureValue(depthReadback, 0xEE000000, 0xFF000000));
// }

} // namespace vex
