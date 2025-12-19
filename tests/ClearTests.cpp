#include "VexTest.h"

namespace vex
{
struct ClearTest : VexTest
{
};

template <class T>
bool ValidateTextureValue(const TextureReadbackContext& ctx, T expectedValue)
{
    std::vector<T> texels;
    texels.resize(ctx.GetDataByteSize() / sizeof(T));
    ctx.ReadData(std::as_writable_bytes(std::span{ texels }));
    return std::ranges::all_of(texels, [&](auto v) { return v == expectedValue; });
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

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture });

    TextureReadbackContext readbackColorCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Color));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue(readbackColorCtx, std::array<u8, 4>{ 0xFF, 0xFF, 0xFF, 0xFF }));
}

TEST_F(ClearTest, ClearRenderTargetExplicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestRenderTarget",
                                                                           TextureFormat::BGRA8_UNORM,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::RenderTarget));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{ .flags = TextureClear::ClearColor, .color = { 1, 1, 1, 1 } });

    TextureReadbackContext readbackColorCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Color));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue(readbackColorCtx, std::array<u8, 4>{ 0xFF, 0xFF, 0xFF, 0xFF }));
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

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture });

    TextureReadbackContext readbackDepthCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Depth));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<float>(readbackDepthCtx, 0.54f));
}

TEST_F(ClearTest, ClearDepthOnlyExpicit)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                           TextureFormat::D32_FLOAT,
                                                                           10,
                                                                           10,
                                                                           1,
                                                                           TextureUsage::DepthStencil));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{
                         .flags = TextureClear::ClearDepth,
                         .depth = 0.54f,
                     });

    TextureReadbackContext readbackDepthCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Depth));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<float>(readbackDepthCtx, 0.54f));
}

TEST_F(ClearTest, ClearDepthStencilImplicit)
{
    auto texture = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                         TextureFormat::D24_UNORM_S8_UINT,
                                         10,
                                         10,
                                         1,
                                         TextureUsage::DepthStencil | TextureUsage::ShaderRead,
                                         TextureClearValue{
                                             .flags = TextureClear::ClearStencil | TextureClear::ClearDepth,
                                             .depth = .54f,
                                             .stencil = 0xEE,
                                         }));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture });

    TextureReadbackContext readbackDepthCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Depth));
    TextureReadbackContext readbackStencilCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Stencil));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<u32>(readbackDepthCtx, static_cast<u32>(0xFFFFFF * 0.54f)));
    EXPECT_TRUE(ValidateTextureValue<u8>(readbackStencilCtx, 0xEE));
}

TEST_F(ClearTest, ClearDepthStencilExplicit)
{
    auto texture =
        graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                TextureFormat::D24_UNORM_S8_UINT,
                                                                10,
                                                                10,
                                                                1,
                                                                TextureUsage::DepthStencil | TextureUsage::ShaderRead));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{
                         .flags = TextureClear::ClearStencil | TextureClear::ClearDepth,
                         .depth = 1.0f,
                         .stencil = 0xEE,
                     });

    TextureReadbackContext readbackDepthCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Depth));
    TextureReadbackContext readbackStencilCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Stencil));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<u32>(readbackDepthCtx, 0xFFFFFF));
    EXPECT_TRUE(ValidateTextureValue<u8>(readbackStencilCtx, 0xEE));
}

TEST_F(ClearTest, ClearStencilImplicit)
{
    auto texture =
        graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                TextureFormat::D24_UNORM_S8_UINT,
                                                                10,
                                                                10,
                                                                1,
                                                                TextureUsage::DepthStencil | TextureUsage::ShaderRead,
                                                                TextureClearValue{
                                                                    .flags = TextureClear::ClearStencil,
                                                                    .stencil = 0xEE,
                                                                }));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture });

    TextureReadbackContext readbackStencilCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Stencil));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<u8>(readbackStencilCtx, 0xEE));
}

TEST_F(ClearTest, ClearStencilExplicit)
{
    auto texture =
        graphics.CreateTexture(TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                                TextureFormat::D24_UNORM_S8_UINT,
                                                                10,
                                                                10,
                                                                1,
                                                                TextureUsage::DepthStencil | TextureUsage::ShaderRead));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{
                         .flags = TextureClear::ClearStencil,
                         .stencil = 0xEE,
                     });

    TextureReadbackContext readbackStencilCtx =
        ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0, TextureAspect::Stencil));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue<u8>(readbackStencilCtx, 0xEE));
}

TEST_F(ClearTest, ClearDepthOnlyRect)
{
    auto texture = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("TestRenderTarget",
                                         TextureFormat::D32_FLOAT,
                                         10,
                                         10,
                                         1,
                                         TextureUsage::DepthStencil,
                                         TextureClearValue{ .flags = TextureClear::ClearDepth, .depth = 0.0f }));

    std::array clearRects{
        TextureClearRect{ .extentX = 5, .extentY = 5 },
        TextureClearRect{ .offsetX = 5, .offsetY = 5, .extentX = 5, .extentY = 5 },
    };

    auto ctx = graphics.CreateCommandContext(QueueType::Graphics);

    // Need to clear full texture first because a partial clear doesnt count as valid init
    ctx.ClearTexture({ .texture = texture }, texture.desc.clearValue);
    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{ .flags = TextureClear::ClearDepth, .depth = 0.7f },
                     clearRects);

    auto readbackTopLeftCtx = ctx.EnqueueDataReadback(texture,
                                                      TextureRegion{
                                                          .subresource = { .aspect = TextureAspect::Depth },
                                                          .offset = { 0, 0 },
                                                          .extent = { 5, 5 },
                                                      });

    auto readbackBottomRightCtx = ctx.EnqueueDataReadback(texture,
                                                          TextureRegion{
                                                              .subresource = { .aspect = TextureAspect::Depth },
                                                              .offset = { 5, 5 },
                                                              .extent = { 5, 5 },
                                                          });

    auto readbackBottomLeftCtx = ctx.EnqueueDataReadback(texture,
                                                         TextureRegion{
                                                             .subresource = { .aspect = TextureAspect::Depth },
                                                             .offset = { 0, 5 },
                                                             .extent = { 5, 5 },
                                                         });
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue(readbackTopLeftCtx, 0.7f));
    EXPECT_TRUE(ValidateTextureValue(readbackBottomRightCtx, 0.7f));
    EXPECT_TRUE(ValidateTextureValue(readbackBottomLeftCtx, 0.0f));
}

TEST_F(ClearTest, ClearRenderTargetRect)
{
    auto texture = graphics.CreateTexture(TextureDesc::CreateTexture2DDesc(
        "TestRenderTarget",
        TextureFormat::BGRA8_UNORM,
        10,
        10,
        1,
        TextureUsage::RenderTarget,
        TextureClearValue{ .flags = TextureClear::ClearColor, .color = { 0, 0, 0, 0 } }));

    std::array clearRects{
        TextureClearRect{ .extentX = 5, .extentY = 5 },
        TextureClearRect{ .offsetX = 5, .offsetY = 5, .extentX = 5, .extentY = 5 },
    };

    auto ctx = graphics.CreateCommandContext(QueueType::Graphics);

    // Need to clear full texture first because a partial clear doesnt count as valid init
    ctx.ClearTexture({ .texture = texture }, texture.desc.clearValue);
    ctx.ClearTexture({ .texture = texture },
                     TextureClearValue{ .flags = TextureClear::ClearColor, .color = { 1, 0, 1, 0 } },
                     clearRects);

    auto readbackTopLeftCtx = ctx.EnqueueDataReadback(texture,
                                                      TextureRegion{
                                                          .offset = { 0, 0 },
                                                          .extent = { 5, 5 },
                                                      });

    auto readbackBottomRightCtx = ctx.EnqueueDataReadback(texture,
                                                          TextureRegion{
                                                              .offset = { 5, 5 },
                                                              .extent = { 5, 5 },
                                                          });

    auto readbackBottomLeftCtx = ctx.EnqueueDataReadback(texture,
                                                         TextureRegion{
                                                             .offset = { 0, 5 },
                                                             .extent = { 5, 5 },
                                                         });
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    EXPECT_TRUE(ValidateTextureValue(readbackTopLeftCtx, std::array<u8, 4>{ 0xFF, 0x00, 0xFF, 0x00 }));
    EXPECT_TRUE(ValidateTextureValue(readbackBottomRightCtx, std::array<u8, 4>{ 0xFF, 0x00, 0xFF, 0x00 }));
    EXPECT_TRUE(ValidateTextureValue(readbackBottomLeftCtx, std::array<u8, 4>{ 0, 0, 0, 0 }));
}

} // namespace vex
