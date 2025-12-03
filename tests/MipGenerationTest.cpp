#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex.h>
#include <Vex/Utility/ByteUtils.h>

namespace vex
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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(width, height), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);

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

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, greenData, TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(size, size, 8), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback multiple mip levels to verify the chain
    std::vector<TextureReadbackContext> readbacks;
    for (u16 mip = 0; mip < numMips; ++mip)
    {
        readbacks.push_back(ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(mip)));
    }

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA32(width, height, 2), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = false });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA8(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    // Readback the last mip (1x1).
    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGBA8(size, size), TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, greenData, TextureRegion::SingleMip(0));

    ctx.GenerateMips(TextureBinding{ .texture = tex, .isSRGB = true });

    TextureReadbackContext readback = ctx.EnqueueDataReadback(tex, TextureRegion::SingleMip(numMips - 1));

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);

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

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);

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

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);

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

    graphics.WaitForTokenOnCPU(ctx.Submit());

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

} // namespace vex
