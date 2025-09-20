#include <Vex.h>

namespace vex
{

void TestTextureUpload(NonNullPtr<GfxBackend> graphics, CommandQueueType type)
{

    // Test 1: Upload a cubemap with two mips
    {
        auto ctx = graphics->BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        u32 cubemapFaceSize = 16;
        u16 cubemapMips = 2;
        Texture cubemapTexture =
            graphics->CreateTexture(TextureDescription::CreateTextureCubeDesc("Cubemap",
                                                                              TextureFormat::RGBA8_UNORM,
                                                                              cubemapFaceSize,
                                                                              cubemapMips));

        std::vector<u8> cubemapData;
        cubemapData.reserve(cubemapFaceSize * cubemapFaceSize * GTextureCubeFaceCount * 4 * cubemapMips);
        for (u8 face = 0; face < GTextureCubeFaceCount; ++face)
        {
            for (unsigned int x = 0; x < cubemapFaceSize; ++x)
            {
                for (unsigned int y = 0; y < cubemapFaceSize; ++y)
                {
                    cubemapData.push_back(32 * face);
                    cubemapData.push_back(64);
                    cubemapData.push_back(128);
                    cubemapData.push_back(255);
                }
            }
        }
        cubemapFaceSize /= 2;
        for (u8 face = 0; face < GTextureCubeFaceCount; ++face)
        {
            for (unsigned int x = 0; x < cubemapFaceSize; ++x)
            {
                for (unsigned int y = 0; y < cubemapFaceSize; ++y)
                {
                    cubemapData.push_back((face % 2 == 0) * 255);
                    cubemapData.push_back(0);
                    cubemapData.push_back(0);
                    cubemapData.push_back(255);
                }
            }
        }

        ctx.EnqueueDataUpload(cubemapTexture,
                              std::as_bytes(std::span(cubemapData)),
                              TextureRegion::AllMips(cubemapTexture.description));

        ctx.Submit();
        graphics->DestroyTexture(cubemapTexture);
    }

    // Test 2: Upload a 2d texture array of size 2 with 3 mips
    {
        auto ctx = graphics->BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        u32 width = 16, height = 12, arraySize = 2;
        u16 mips = 3;
        Texture texture =
            graphics->CreateTexture(TextureDescription::CreateTexture2DArrayDesc("2dTextureArray",
                                                                                 TextureFormat::RGBA8_UNORM,
                                                                                 width,
                                                                                 height,
                                                                                 arraySize,
                                                                                 mips));

        std::vector<u8> data;
        data.reserve(width * height * arraySize * 4 * mips);
        for (u8 slice = 0; slice < arraySize; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back(32 * slice);
                    data.push_back(64);
                    data.push_back(128);
                    data.push_back(255);
                }
            }
        }
        width /= 2;
        height /= 2;
        for (u8 slice = 0; slice < arraySize; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back((slice % 2 == 0) * 255);
                    data.push_back(0);
                    data.push_back(0);
                    data.push_back(255);
                }
            }
        }
        width /= 2;
        height /= 2;
        for (u8 slice = 0; slice < arraySize; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back(255);
                    data.push_back((slice % 2 == 0) * 255);
                    data.push_back((slice % 2 != 0) * 255);
                    data.push_back(255);
                }
            }
        }

        ctx.EnqueueDataUpload(texture,
                              std::as_bytes(std::span(data)),
                              TextureRegion::AllMips(texture.description));
        ctx.Submit();
        graphics->DestroyTexture(texture);
    }

    // Test 3: Upload a texture cube array of size 3 with 2 mips
    {
        auto ctx = graphics->BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        u32 cubemapFaceSize = 16;
        u16 cubemapMips = 2;
        u16 cubemapArraySize = 3;
        Texture cubemapTexture =
            graphics->CreateTexture(TextureDescription::CreateTextureCubeArrayDesc("CubemapArray",
                                                                                   TextureFormat::RGBA8_UNORM,
                                                                                   cubemapFaceSize,
                                                                                   cubemapArraySize,
                                                                                   cubemapMips));

        std::vector<u8> cubemapData;
        cubemapData.reserve(cubemapFaceSize * cubemapFaceSize * GTextureCubeFaceCount * cubemapArraySize * 4 *
                            cubemapMips);
        for (u8 face = 0; face < GTextureCubeFaceCount; ++face)
        {
            for (u32 slice = 0; slice < cubemapArraySize; ++slice)
            {
                for (unsigned int x = 0; x < cubemapFaceSize; ++x)
                {
                    for (unsigned int y = 0; y < cubemapFaceSize; ++y)
                    {
                        cubemapData.push_back(255);
                        cubemapData.push_back(32 * slice);
                        cubemapData.push_back(128);
                        cubemapData.push_back(255);
                    }
                }
            }
        }
        cubemapFaceSize /= 2;
        for (u8 face = 0; face < GTextureCubeFaceCount; ++face)
        {
            for (u32 slice = 0; slice < cubemapArraySize; ++slice)
            {
                for (unsigned int x = 0; x < cubemapFaceSize; ++x)
                {
                    for (unsigned int y = 0; y < cubemapFaceSize; ++y)
                    {
                        cubemapData.push_back((face % 2 == 0) * 255);
                        cubemapData.push_back(0);
                        cubemapData.push_back(0);
                        cubemapData.push_back(255);
                    }
                }
            }
        }

        ctx.EnqueueDataUpload(cubemapTexture,
                              std::as_bytes(std::span(cubemapData)),
                              TextureRegion::AllMips(cubemapTexture.description));

        ctx.Submit();
        graphics->DestroyTexture(cubemapTexture);
    }

    // Test 4: Upload a 3d texture of depth 2 with 3 mips
    {
        auto ctx = graphics->BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        // Cursed non-even sizes.
        u32 width = 121, height = 165, depth = 64;
        u16 mips = 3;
        Texture texture = graphics->CreateTexture(TextureDescription::CreateTexture3DDesc("3DTexture",
                                                                                          TextureFormat::RGBA8_UNORM,
                                                                                          width,
                                                                                          height,
                                                                                          depth,
                                                                                          mips));

        std::vector<u8> data;
        data.reserve(width * height * depth * 4 * mips);
        for (u8 slice = 0; slice < depth; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back(32 * slice);
                    data.push_back(64);
                    data.push_back(128);
                    data.push_back(255);
                }
            }
        }
        width /= 2;
        height /= 2;
        depth /= 2;
        for (u8 slice = 0; slice < depth; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back((slice % 2 == 0) * 255);
                    data.push_back(0);
                    data.push_back(0);
                    data.push_back(255);
                }
            }
        }
        width /= 2;
        height /= 2;
        depth /= 2;
        for (u8 slice = 0; slice < depth; ++slice)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                for (unsigned int y = 0; y < height; ++y)
                {
                    data.push_back(255);
                    data.push_back((slice % 2 == 0) * 255);
                    data.push_back((slice % 2 != 0) * 255);
                    data.push_back(255);
                }
            }
        }

        ctx.EnqueueDataUpload(texture,
                              std::as_bytes(std::span(data)),
                              TextureRegion::AllMips(texture.description));
        ctx.Submit();
        graphics->DestroyTexture(texture);
    }

    graphics->FlushGPU();
}

} // namespace vex