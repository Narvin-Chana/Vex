#include "VexTest.h"

#include <gtest/gtest.h>

namespace vex
{

namespace MipGenerationTests
{

struct MipGenerationTest : public VexTest
{
public:
    static std::vector<byte> Generate2DCheckerboardRGBA32(u32 width, u32 height, u32 checkerSize = 64)
    {
        VEX_ASSERT(checkerSize <= width && checkerSize <= height);
        // RGBA32_FLOAT means 4 floats per pixel (16 bytes).
        static constexpr u32 FloatsPerPixel = 4;
        std::vector<byte> data(width * height * sizeof(float[FloatsPerPixel]));

        float* floatData = reinterpret_cast<float*>(data.data());

        for (u32 y = 0; y < height; ++y)
        {
            for (u32 x = 0; x < width; ++x)
            {
                u32 pixelIndex = (y * width + x) * FloatsPerPixel;

                // Determine which checker square we're in
                u32 checkerX = x / checkerSize;
                u32 checkerY = y / checkerSize;
                bool isRed = ((checkerX + checkerY) % 2) == 0;

                if (isRed)
                {
                    floatData[pixelIndex + 0] = 1.0f; // R
                    floatData[pixelIndex + 1] = 0.0f; // G
                    floatData[pixelIndex + 2] = 0.0f; // B
                    floatData[pixelIndex + 3] = 1.0f; // A
                }
                else
                {
                    floatData[pixelIndex + 0] = 0.0f; // R
                    floatData[pixelIndex + 1] = 0.0f; // G
                    floatData[pixelIndex + 2] = 1.0f; // B
                    floatData[pixelIndex + 3] = 1.0f; // A
                }
            }
        }

        return data;
    }

    static std::vector<byte> Generate2DCheckerboardRGBA8(u32 width, u32 height, u32 checkerSize = 64)
    {
        VEX_ASSERT(checkerSize <= width && checkerSize <= height);
        // RGBA8_UNORM_SRGB means 4 bytes per pixel.
        static constexpr u32 BytesPerPixel = 4;
        std::vector<byte> data(width * height * BytesPerPixel);

        for (u32 y = 0; y < height; ++y)
        {
            for (u32 x = 0; x < width; ++x)
            {
                u32 pixelIndex = (y * width + x) * BytesPerPixel;

                // Determine which checker square we're in
                u32 checkerX = x / checkerSize;
                u32 checkerY = y / checkerSize;
                bool isRed = ((checkerX + checkerY) % 2) == 0;

                if (isRed)
                {
                    data[pixelIndex + 0] = byte(255); // R
                    data[pixelIndex + 1] = byte(0);   // G
                    data[pixelIndex + 2] = byte(0);   // B
                    data[pixelIndex + 3] = byte(255); // A
                }
                else
                {
                    data[pixelIndex + 0] = byte(0);   // R
                    data[pixelIndex + 1] = byte(0);   // G
                    data[pixelIndex + 2] = byte(255); // B
                    data[pixelIndex + 3] = byte(255); // A
                }
            }
        }

        return data;
    }
};

TEST_F(MipGenerationTest, Texture2DPowOfTwo)
{
    u32 size = 512;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("Mip0",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    // The last mip should be the average color of the data we uploaded:
    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    // A 50/50 red-blue checkerboard should average to purple (0.5, 0, 0.5)
    EXPECT_NEAR(r, 0.5f, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(g, 0.0f, 0.01f) << "Final mip green channel should be 0.0";
    EXPECT_NEAR(b, 0.5f, 0.01f) << "Final mip blue channel should be 0.5";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DNonPowOfTwo)
{
    // Use 384 = 64 * 6 for exactly 6x6 = 36 squares (18 red, 18 blue)
    // This allows us to test the non-power of two while still keeping the last mip equal to {0.5, 0, 0.5f, 1}
    u32 size = 384;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("Mip0",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    // The last mip should be the average color of the data we uploaded:
    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    // A 50/50 red-blue checkerboard should average to purple (0.5, 0, 0.5)
    EXPECT_NEAR(r, 0.5f, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(g, 0.0f, 0.01f) << "Final mip green channel should be 0.0";
    EXPECT_NEAR(b, 0.5f, 0.01f) << "Final mip blue channel should be 0.5";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DNonSquare)
{
    u32 width = 512;
    u32 height = 256;
    u16 numMips = ComputeMipCount({ width, height, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("NonSquare",
                                         TextureFormat::RGBA32_FLOAT,
                                         width,
                                         height,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(width, height), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    EXPECT_NEAR(r, 0.5f, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(g, 0.0f, 0.01f) << "Final mip green channel should be 0.0";
    EXPECT_NEAR(b, 0.5f, 0.01f) << "Final mip blue channel should be 0.5";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DWithSourceMipOffset)
{
    u32 size = 8;
    u16 numMips = ComputeMipCount({ size * 2, size * 2, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("Mip1",
                                         TextureFormat::RGBA32_FLOAT,
                                         size * 2,
                                         size * 2,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    float rAvg = 0;
    float bAvg = 0;

    std::vector<float> data(size * size * 4);
    for (u32 x = 0; x < size; ++x)
    {
        for (u32 y = 0; y < size; ++y)
        {
            u32 idx = (x * size + y) * 4;
            data[idx + 0] = (x % 2 == 0) ? 1 : 0;
            data[idx + 1] = 0;
            data[idx + 2] = (y % 2 == 0) ? 1 : 0;
            data[idx + 3] = 1;
            // Compute averages for later on.
            rAvg += data[idx + 0] / (size * size);
            bAvg += data[idx + 2] / (size * size);
        }
    }

    // Upload to mip 1.
    ctx.EnqueueDataUpload(tex, std::as_bytes(std::span(data)), TextureRegion::SingleMip(1));

    // Generate and fill in the remaining mips, using the mip 1 as a sourceMip.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false, .subresource = { .startMip = 1 } });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    // The last mip should be the average color of the data we uploaded:
    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    // A 50/50 red-blue checkerboard should average to purple (0.5, 0, 0.5)
    EXPECT_NEAR(r, rAvg, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(g, 0.0f, 0.01f) << "Final mip green channel should be 0.0";
    EXPECT_NEAR(b, bAvg, 0.01f) << "Final mip blue channel should be 0.5";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DSingleColor)
{
    u32 size = 256;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("SingleColor",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    // Create solid green texture
    std::vector<byte> greenData(size * size * 4 * sizeof(float));
    float* floatData = reinterpret_cast<float*>(greenData.data());
    for (u32 i = 0; i < size * size; ++i)
    {
        floatData[i * 4 + 0] = 0.0f; // R
        floatData[i * 4 + 1] = 1.0f; // G
        floatData[i * 4 + 2] = 0.0f; // B
        floatData[i * 4 + 3] = 1.0f; // A
    }

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, greenData, TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    // Solid green should remain solid green at all mip levels
    EXPECT_NEAR(r, 0.0f, 0.01f) << "Final mip red channel should be 0.0";
    EXPECT_NEAR(g, 1.0f, 0.01f) << "Final mip green channel should be 1.0";
    EXPECT_NEAR(b, 0.0f, 0.01f) << "Final mip blue channel should be 0.0";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DIntermediateMipCheck)
{
    u32 size = 16;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    EXPECT_EQ(numMips, 5) << "16x16 texture should have 5 mip levels";

    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("IntermediateMip",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size, 8), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback multiple mip levels to verify the chain
    std::vector<TextureReadbackContext> readbacks;
    for (u16 mip = 0; mip < numMips; ++mip)
    {
        readbacks.push_back(ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(mip)));
    }

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    // Verify each mip has the correct size
    u32 expectedWidth = size;
    u32 expectedHeight = size;
    for (u16 mip = 0; mip < numMips; ++mip)
    {
        std::vector<byte> mipData(readbacks[mip].GetDataByteSize());
        readbacks[mip].ReadData(mipData);

        u32 expectedSize = expectedWidth * expectedHeight * 4 * sizeof(float);
        EXPECT_EQ(mipData.size(), expectedSize)
            << "Mip " << mip << " should be " << expectedWidth << "x" << expectedHeight;

        expectedWidth = std::max(1u, expectedWidth / 2);
        expectedHeight = std::max(1u, expectedHeight / 2);
    }

    // Final mip should still be the averaged color
    std::vector<byte> finalMipData(readbacks[numMips - 1].GetDataByteSize());
    readbacks[numMips - 1].ReadData(finalMipData);

    float* finalPixel = reinterpret_cast<float*>(finalMipData.data());
    EXPECT_NEAR(finalPixel[0], 0.5f, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(finalPixel[2], 0.5f, 0.01f) << "Final mip blue channel should be 0.5";
}

TEST_F(MipGenerationTest, Texture2DExtremeAspectRatio)
{
    u32 width = 1024;
    u32 height = 4;
    u16 numMips = ComputeMipCount({ width, height, 1 });

    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("ExtremeAspect",
                                         TextureFormat::RGBA32_FLOAT,
                                         width,
                                         height,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(width, height, 2), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    float* finalPixel = reinterpret_cast<float*>(mipData.data());
    float r = finalPixel[0];
    float g = finalPixel[1];
    float b = finalPixel[2];
    float a = finalPixel[3];

    EXPECT_NEAR(r, 0.5f, 0.01f) << "Final mip red channel should be 0.5";
    EXPECT_NEAR(g, 0.0f, 0.01f) << "Final mip green channel should be 0.0";
    EXPECT_NEAR(b, 0.5f, 0.01f) << "Final mip blue channel should be 0.5";
    EXPECT_NEAR(a, 1.0f, 0.01f) << "Final mip alpha channel should be 1.0";
}

TEST_F(MipGenerationTest, Texture2DSRGB)
{
    u32 size = 512;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("SRGB_Mip0",
                                         TextureFormat::RGBA8_UNORM,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA8(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    // The last mip should be the average color of the data we uploaded.
    u8 r = u8(mipData[0]);
    u8 g = u8(mipData[1]);
    u8 b = u8(mipData[2]);
    u8 a = u8(mipData[3]);

    // A 50/50 red-blue checkerboard should average to purple.
    // In sRGB space, averaging red (255,0,0) and blue (0,0,255) doesn't give exactly (127,0,127).
    // because of gamma correction, but should be close.
    // If mip generation is done in linear space (correct), we'd expect ~188 after gamma conversion.
    // If done in sRGB space (incorrect), we'd expect ~127.
    EXPECT_EQ(r, 188) << "Final mip red channel should be 188";
    EXPECT_EQ(g, 0) << "Final mip green channel should be 0";
    EXPECT_EQ(b, 188) << "Final mip blue channel should be 188";
    EXPECT_EQ(a, 255) << "Final mip alpha channel should be 255";
}

TEST_F(MipGenerationTest, Texture2DSRGBNonPowOfTwo)
{
    u32 size = 384;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("SRGB_NonPow2",
                                         TextureFormat::RGBA8_UNORM,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA8(size, size), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    u8 r = u8(mipData[0]);
    u8 g = u8(mipData[1]);
    u8 b = u8(mipData[2]);
    u8 a = u8(mipData[3]);

    // A 50/50 red-blue checkerboard should average to purple.
    // In sRGB space, averaging red (255,0,0) and blue (0,0,255) doesn't give exactly (127,0,127).
    // because of gamma correction, but should be close.
    // If mip generation is done in linear space (correct), we'd expect ~188 after gamma conversion.
    // If done in sRGB space (incorrect), we'd expect ~127.
    EXPECT_EQ(r, 188) << "Final mip red channel should be 188";
    EXPECT_EQ(g, 0) << "Final mip green channel should be 0";
    EXPECT_EQ(b, 188) << "Final mip blue channel should be 188";
    EXPECT_EQ(a, 255) << "Final mip alpha channel should be 255";
}

TEST_F(MipGenerationTest, Texture2DSRGBSingleColor)
{
    u32 size = 256;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("SRGB_SingleColor",
                                         TextureFormat::RGBA8_UNORM,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    // Create solid green texture (RGBA8)
    std::vector<byte> greenData(size * size * 4);
    for (u32 i = 0; i < size * size; ++i)
    {
        greenData[i * 4 + 0] = byte(0);   // R
        greenData[i * 4 + 1] = byte(255); // G
        greenData[i * 4 + 2] = byte(0);   // B
        greenData[i * 4 + 3] = byte(255); // A
    }

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, greenData, TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    u8 r = u8(mipData[0]);
    u8 g = u8(mipData[1]);
    u8 b = u8(mipData[2]);
    u8 a = u8(mipData[3]);

    // Solid green should remain solid green at all mip levels, even with gamma correction.
    EXPECT_EQ(r, 0) << "Final mip red channel should be 0";
    EXPECT_EQ(g, 255) << "Final mip green channel should be 255";
    EXPECT_EQ(b, 0) << "Final mip blue channel should be 0";
    EXPECT_EQ(a, 255) << "Final mip alpha channel should be 255";
}

TEST_F(MipGenerationTest, Texture2DArray)
{
    u32 size = 256;
    u32 arraySize = 4;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DArrayDesc("Array",
                                              TextureFormat::RGBA32_FLOAT,
                                              size,
                                              size,
                                              arraySize,
                                              numMips,
                                              TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    // Upload checkerboard to each slice
    for (u32 slice = 0; slice < arraySize; ++slice)
    {
        TextureRegion region = TextureRegion::SingleMip(0);
        region.subresource.startSlice = slice;
        region.subresource.sliceCount = 1;
        ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size), region);
    }

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1) from each slice
    std::vector<TextureReadbackContext> readbacks;
    for (u32 slice = 0; slice < arraySize; ++slice)
    {
        TextureRegion region = TextureRegion::SingleMip(numMips - 1);
        region.subresource.startSlice = slice;
        region.subresource.sliceCount = 1;
        readbacks.push_back(ctx.EnqueueDataReadback(tex, region));
    }

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    // Verify each slice averaged correctly
    for (u32 slice = 0; slice < arraySize; ++slice)
    {
        std::vector<byte> mipData(readbacks[slice].GetDataByteSize());
        readbacks[slice].ReadData(mipData);

        float* finalPixel = reinterpret_cast<float*>(mipData.data());
        float r = finalPixel[0];
        float g = finalPixel[1];
        float b = finalPixel[2];
        float a = finalPixel[3];

        EXPECT_NEAR(r, 0.5f, 0.01f) << "Slice " << slice << " final mip red channel should be 0.5";
        EXPECT_NEAR(g, 0.0f, 0.01f) << "Slice " << slice << " final mip green channel should be 0.0";
        EXPECT_NEAR(b, 0.5f, 0.01f) << "Slice " << slice << " final mip blue channel should be 0.5";
        EXPECT_NEAR(a, 1.0f, 0.01f) << "Slice " << slice << " final mip alpha channel should be 1.0";
    }
}

TEST_F(MipGenerationTest, TextureCube)
{
    u32 faceSize = 256;
    u16 numMips = ComputeMipCount({ faceSize, faceSize, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTextureCubeDesc("Cube",
                                           TextureFormat::RGBA32_FLOAT,
                                           faceSize,
                                           numMips,
                                           TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    // Upload checkerboard to each of the 6 cube faces
    for (u32 face = 0; face < 6; ++face)
    {
        TextureRegion region = TextureRegion::SingleMip(0);
        region.subresource.startSlice = face;
        region.subresource.sliceCount = 1;
        ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(faceSize, faceSize), region);
    }

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1) from each face
    std::vector<TextureReadbackContext> readbacks;
    for (u32 face = 0; face < 6; ++face)
    {
        TextureRegion region = TextureRegion::SingleMip(numMips - 1);
        region.subresource.startSlice = face;
        region.subresource.sliceCount = 1;
        readbacks.push_back(ctx.EnqueueDataReadback(tex, region));
    }

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    // Verify each face averaged correctly
    for (u32 face = 0; face < 6; ++face)
    {
        std::vector<byte> mipData(readbacks[face].GetDataByteSize());
        readbacks[face].ReadData(mipData);

        float* finalPixel = reinterpret_cast<float*>(mipData.data());
        float r = finalPixel[0];
        float g = finalPixel[1];
        float b = finalPixel[2];
        float a = finalPixel[3];

        EXPECT_NEAR(r, 0.5f, 0.01f) << "Face " << face << " final mip red channel should be 0.5";
        EXPECT_NEAR(g, 0.0f, 0.01f) << "Face " << face << " final mip green channel should be 0.0";
        EXPECT_NEAR(b, 0.5f, 0.01f) << "Face " << face << " final mip blue channel should be 0.5";
        EXPECT_NEAR(a, 1.0f, 0.01f) << "Face " << face << " final mip alpha channel should be 1.0";
    }
}

TEST_F(MipGenerationTest, TextureCubeArray)
{
    u32 faceSize = 128;
    u32 cubeCount = 3;
    u16 numMips = ComputeMipCount({ faceSize, faceSize, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTextureCubeArrayDesc("CubeArray",
                                                TextureFormat::RGBA32_FLOAT,
                                                faceSize,
                                                cubeCount,
                                                numMips,
                                                TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    // Upload checkerboard to each face of each cube (cubeCount * 6 total slices)
    u32 totalSlices = cubeCount * 6;
    for (u32 slice = 0; slice < totalSlices; ++slice)
    {
        TextureRegion region = TextureRegion::SingleMip(0);
        region.subresource.startSlice = slice;
        region.subresource.sliceCount = 1;
        ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(faceSize, faceSize), region);
    }

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1) from each slice
    std::vector<TextureReadbackContext> readbacks;
    for (u32 slice = 0; slice < totalSlices; ++slice)
    {
        TextureRegion region = TextureRegion::SingleMip(numMips - 1);
        region.subresource.startSlice = slice;
        region.subresource.sliceCount = 1;
        readbacks.push_back(ctx.EnqueueDataReadback(tex, region));
    }

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    // Verify each slice averaged correctly
    for (u32 slice = 0; slice < totalSlices; ++slice)
    {
        std::vector<byte> mipData(readbacks[slice].GetDataByteSize());
        readbacks[slice].ReadData(mipData);

        float* finalPixel = reinterpret_cast<float*>(mipData.data());
        float r = finalPixel[0];
        float g = finalPixel[1];
        float b = finalPixel[2];
        float a = finalPixel[3];

        u32 cubeIndex = slice / 6;
        u32 faceIndex = slice % 6;

        EXPECT_NEAR(r, 0.5f, 0.01f) << "Cube " << cubeIndex << " Face " << faceIndex << " red should be 0.5";
        EXPECT_NEAR(g, 0.0f, 0.01f) << "Cube " << cubeIndex << " Face " << faceIndex << " green should be 0.0";
        EXPECT_NEAR(b, 0.5f, 0.01f) << "Cube " << cubeIndex << " Face " << faceIndex << " blue should be 0.5";
        EXPECT_NEAR(a, 1.0f, 0.01f) << "Cube " << cubeIndex << " Face " << faceIndex << " alpha should be 1.0";
    }
}

} // namespace MipGenerationTests

// ---------------------------------------------------------------------------------------------------------------
// Second batch of tests to make sure that the mip generation shader correctly handles various formats.

struct FormatTestDesc
{
    std::string_view name;
    TextureFormat format;
    bool isSRGB;

    std::function<std::vector<byte>(u32 width, u32 height)> generator;
    std::function<std::array<float, 4>(const std::vector<byte>&)> reader;

    std::array<float, 4> expectedResult;
    float tolerance;
};

namespace ByteUtils
{

// Sourced from https://stackoverflow.com/a/76816560
u16 FloatToHalf(float a)
{
    uint32_t ia = *reinterpret_cast<uint32_t*>(&a);
    uint16_t ir;

    ir = (ia >> 16) & 0x8000;
    if ((ia & 0x7f800000) == 0x7f800000)
    {
        if ((ia & 0x7fffffff) == 0x7f800000)
        {
            ir |= 0x7c00; /* infinity */
        }
        else
        {
            ir |= 0x7e00 | ((ia >> (24 - 11)) & 0x1ff); /* NaN, quietened */
        }
    }
    else if ((ia & 0x7f800000) >= 0x33000000)
    {
        int shift = (int)((ia >> 23) & 0xff) - 127;
        if (shift > 15)
        {
            ir |= 0x7c00; /* infinity */
        }
        else
        {
            ia = (ia & 0x007fffff) | 0x00800000; /* extract mantissa */
            if (shift < -14)
            { /* denormal */
                ir |= ia >> (-1 - shift);
                ia = ia << (32 - (-1 - shift));
            }
            else
            { /* normal */
                ir |= ia >> (24 - 11);
                ia = ia << (32 - (24 - 11));
                ir = ir + ((14 + shift) << 10);
            }
            /* IEEE-754 round to nearest of even */
            if ((ia > 0x80000000) || ((ia == 0x80000000) && (ir & 1)))
            {
                ir++;
            }
        }
    }
    return ir;
}

// Generated with Claude, Warning!! Doesn't handle denormalized values!!
static float HalfToFloat(u16 h)
{
    u32 bits = ((h & 0x8000u) << 16) | (((h & 0x7c00u) + 0x1C000u) << 13) | ((h & 0x03ffu) << 13);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

} // namespace ByteUtils

// Claude-generated generators and readers.

namespace generators
{

static std::vector<byte> MakeCheckerRGBA32F(u32 w, u32 h)
{
    std::vector<byte> data(w * h * 4 * sizeof(float));
    float* p = reinterpret_cast<float*>(data.data());
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isRed = ((x / 64 + y / 64) % 2) == 0;
            u32 i = (y * w + x) * 4;
            p[i + 0] = isRed ? 1.f : 0.f;
            p[i + 1] = 0.f;
            p[i + 2] = isRed ? 0.f : 1.f;
            p[i + 3] = 1.f;
        }
    return data;
}

static std::vector<byte> MakeCheckerRG32F(u32 w, u32 h)
{
    std::vector<byte> data(w * h * 2 * sizeof(float));
    float* p = reinterpret_cast<float*>(data.data());
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isRed = ((x / 64 + y / 64) % 2) == 0;
            u32 i = (y * w + x) * 2;
            p[i + 0] = isRed ? 1.f : 0.f; // R
            p[i + 1] = isRed ? 0.f : 1.f; // G — blue mapped to G for RG format
        }
    return data;
}

static std::vector<byte> MakeCheckerR32F(u32 w, u32 h)
{
    std::vector<byte> data(w * h * sizeof(float));
    float* p = reinterpret_cast<float*>(data.data());
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isA = ((x / 64 + y / 64) % 2) == 0;
            p[y * w + x] = isA ? 1.f : 0.f;
        }
    return data;
}

static std::vector<byte> MakeCheckerRGBA16F(u32 w, u32 h)
{
    std::vector<byte> data(w * h * 4 * sizeof(u16));
    u16* p = reinterpret_cast<u16*>(data.data());
    auto ToHalf = [](float f) -> u16 { return ByteUtils::FloatToHalf(f); };
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isRed = ((x / 64 + y / 64) % 2) == 0;
            u32 i = (y * w + x) * 4;
            p[i + 0] = ToHalf(isRed ? 1.f : 0.f);
            p[i + 1] = ToHalf(0.f);
            p[i + 2] = ToHalf(isRed ? 0.f : 1.f);
            p[i + 3] = ToHalf(1.f);
        }
    return data;
}

static std::vector<byte> MakeCheckerRGBA8(u32 w, u32 h)
{
    std::vector<byte> data(w * h * 4);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isRed = ((x / 64 + y / 64) % 2) == 0;
            u32 i = (y * w + x) * 4;
            data[i + 0] = byte(isRed ? 255 : 0);
            data[i + 1] = byte(0);
            data[i + 2] = byte(isRed ? 0 : 255);
            data[i + 3] = byte(255);
        }
    return data;
}

static std::vector<byte> MakeCheckerRG8(u32 w, u32 h)
{
    std::vector<byte> data(w * h * 2);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isA = ((x / 64 + y / 64) % 2) == 0;
            u32 i = (y * w + x) * 2;
            data[i + 0] = byte(isA ? 255 : 0);
            data[i + 1] = byte(isA ? 0 : 255);
        }
    return data;
}

static std::vector<byte> MakeCheckerR8(u32 w, u32 h)
{
    std::vector<byte> data(w * h);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x)
        {
            bool isA = ((x / 64 + y / 64) % 2) == 0;
            data[y * w + x] = byte(isA ? 255 : 0);
        }
    return data;
}

} // namespace generators

namespace readers
{
static std::array<float, 4> ReadRGBA32F(const std::vector<byte>& d)
{
    const float* p = reinterpret_cast<const float*>(d.data());
    return { p[0], p[1], p[2], p[3] };
}

static std::array<float, 4> ReadRG32F(const std::vector<byte>& d)
{
    const float* p = reinterpret_cast<const float*>(d.data());
    return { p[0], p[1], 0.f, 1.f };
}

static std::array<float, 4> ReadR32F(const std::vector<byte>& d)
{
    const float* p = reinterpret_cast<const float*>(d.data());
    return { p[0], 0.f, 0.f, 1.f };
}

static std::array<float, 4> ReadRGBA16F(const std::vector<byte>& d)
{
    const u16* p = reinterpret_cast<const u16*>(d.data());
    return {
        ByteUtils::HalfToFloat(p[0]),
        ByteUtils::HalfToFloat(p[1]),
        ByteUtils::HalfToFloat(p[2]),
        ByteUtils::HalfToFloat(p[3]),
    };
}

static std::array<float, 4> ReadRGBA8(const std::vector<byte>& d)
{
    return {
        u8(d[0]) / 255.f,
        u8(d[1]) / 255.f,
        u8(d[2]) / 255.f,
        u8(d[3]) / 255.f,
    };
}

static std::array<float, 4> ReadRG8(const std::vector<byte>& d)
{
    return { u8(d[0]) / 255.f, u8(d[1]) / 255.f, 0.f, 1.f };
}

static std::array<float, 4> ReadR8(const std::vector<byte>& d)
{
    return { u8(d[0]) / 255.f, 0.f, 0.f, 1.f };
}

} // namespace readers

struct MipGenerationFormatTest
    : public VexTest
    , public ::testing::WithParamInterface<FormatTestDesc>
{
};

TEST_P(MipGenerationFormatTest, FinalMipAveragesCorrectly)
{
    const FormatTestDesc& desc = GetParam();

    // Use a size guaranteed to produce a clean 50/50 average:
    // 512 gives many mip levels and the checkerboard tiles evenly.
    constexpr u32 size = 512;
    u16 numMips = ComputeMipCount({ size, size, 1 });

    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc(std::string(desc.name),
                                         desc.format,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);
    ctx.EnqueueDataUpload(tex, desc.generator(size, size), TextureRegion::SingleMip(0));
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = desc.isSRGB });

    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    std::vector<byte> mipData(readback.GetDataByteSize());
    readback.ReadData(mipData);

    auto [r, g, b, a] = desc.reader(mipData);

    EXPECT_NEAR(r, desc.expectedResult[0], desc.tolerance) << desc.name << ": R channel";
    EXPECT_NEAR(g, desc.expectedResult[1], desc.tolerance) << desc.name << ": G channel";
    EXPECT_NEAR(b, desc.expectedResult[2], desc.tolerance) << desc.name << ": B channel";
    EXPECT_NEAR(a, desc.expectedResult[3], desc.tolerance) << desc.name << ": A channel";
}

using namespace generators;
using namespace readers;

static const std::array FormatCases = {
    // --- 32-bit float ---
    FormatTestDesc{
        "RGBA32F_Linear",
        TextureFormat::RGBA32_FLOAT,
        false,
        MakeCheckerRGBA32F,
        ReadRGBA32F,
        { 0.5f, 0.f, 0.5f, 1.f },
        0.01f,
    },
    FormatTestDesc{
        "RG32F_Linear",
        TextureFormat::RG32_FLOAT,
        false,
        MakeCheckerRG32F,
        ReadRG32F,
        { 0.5f, 0.5f, 0.f, 1.f },
        0.01f,
    },
    FormatTestDesc{
        "R32F_Linear",
        TextureFormat::R32_FLOAT,
        false,
        MakeCheckerR32F,
        ReadR32F,
        { 0.5f, 0.f, 0.f, 1.f },
        0.01f,
    },
    // --- 16-bit float ---
    FormatTestDesc{
        "RGBA16F_Linear",
        TextureFormat::RGBA16_FLOAT,
        false,
        MakeCheckerRGBA16F,
        ReadRGBA16F,
        { 0.5f, 0.f, 0.5f, 1.f },
        0.01f,
    },
    // --- 8-bit unorm (linear) ---
    FormatTestDesc{
        "RGBA8_Linear",
        TextureFormat::RGBA8_UNORM,
        false,
        MakeCheckerRGBA8,
        ReadRGBA8,
        { 0.5f, 0.f, 0.5f, 1.f },
        0.01f,
    },
    FormatTestDesc{
        "RG8_Linear",
        TextureFormat::RG8_UNORM,
        false,
        MakeCheckerRG8,
        ReadRG8,
        { 0.5f, 0.5f, 0.f, 1.f },
        0.01f,
    },
    FormatTestDesc{
        "R8_Linear",
        TextureFormat::R8_UNORM,
        false,
        MakeCheckerR8,
        ReadR8,
        { 0.5f, 0.f, 0.f, 1.f },
        0.01f,
    },
    // --- 8-bit unorm with sRGB mip generation ---
    // Input is raw 8-bit (0 or 255), read back normalized.
    // Correct linear-space averaging of sRGB 0 and 1 re-encodes to ~0.735.
    FormatTestDesc{
        "RGBA8_SRGB",
        TextureFormat::RGBA8_UNORM,
        true,
        MakeCheckerRGBA8,
        ReadRGBA8,
        { 0.735f, 0.f, 0.735f, 1.f },
        0.01f,
    },
};

INSTANTIATE_TEST_SUITE_P(AllFormats,
                         MipGenerationFormatTest,
                         ::testing::ValuesIn(FormatCases),
                         [](const ::testing::TestParamInfo<FormatTestDesc>& info)
                         { return std::string(info.param.name); });

} // namespace vex
