#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex.h>
#include <Vex/ByteUtils.h>

namespace vex
{

struct MipGenerationTest : public VexTest
{
public:
    static std::vector<byte> Generate2DCheckerboardRGB(u32 width, u32 height, u32 checkerSize = 64)
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
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGB(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(tex);

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
    u32 size = 519;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("Mip0",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGB(size, size), TextureRegion::SingleMip(0));

    // Generate and fill in the remaining mips.
    ctx.GenerateMips(tex);

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

TEST_F(MipGenerationTest, Texture2DWithSourceMipOffset)
{
    u32 size = 8;
    u16 numMips = ComputeMipCount({ size, size, 1 });
    Texture tex = graphics.CreateTexture(
        TextureDesc::CreateTexture2DDesc("Mip1",
                                         TextureFormat::RGBA32_FLOAT,
                                         size,
                                         size,
                                         numMips,
                                         TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite));

    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    // Upload to mip 1.
    ctx.EnqueueDataUpload(tex, Generate2DCheckerboardRGB(size / 2, size / 2, 4), TextureRegion::SingleMip(1));

    // Generate and fill in the remaining mips, using the mip 1 as a sourceMip.
    ctx.GenerateMips(tex, 1);

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

} // namespace vex
