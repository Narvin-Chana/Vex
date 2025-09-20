#include "TextureUploadReadback.h"

#include <Vex/CommandContext.h>
#include <Vex/GfxBackend.h>
#include <Vex/Logger.h>
#include <Vex/Types.h>

namespace vex
{

namespace TextureTests
{
using PixelApplicator = std::function<void(const TextureRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)>;

struct GridParams
{
    std::array<u8, 4> gridColorA;
    std::array<u8, 4> gridColorB;
    u32 gridCellSize;
};

static constexpr GridParams DefaultGridParams{ .gridColorA = { 0xff, 0xff, 0, 0xff },
                                               .gridColorB = { 0xff, 0, 0xff, 0xff },
                                               .gridCellSize = 32 };

PixelApplicator GenerateGrid(const GridParams& gridArgs)
{
    return [=](const TextureRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
    {
        bool evenX = (x / gridArgs.gridCellSize) % 2 == 0;
        bool evenY = (y / gridArgs.gridCellSize) % 2 == 0;
        bool evenZ = (z / gridArgs.gridCellSize) % 2 == 0;

        for (int i = 0; i < 4; ++i)
        {
            pixel[i] = static_cast<byte>(evenX ^ evenY ^ evenZ ? gridArgs.gridColorA[i] : gridArgs.gridColorB[i]);
        }
    };
}

PixelApplicator ValidateGrid(const GridParams& grid)
{
    return [=](const TextureRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
    {
        bool evenX = (x / grid.gridCellSize) % 2 == 0;
        bool evenY = (y / grid.gridCellSize) % 2 == 0;
        bool evenZ = (z / grid.gridCellSize) % 2 == 0;

        for (int i = 0; i < 4; ++i)
        {
            VEX_ASSERT(pixel[i] == static_cast<byte>(evenX ^ evenY ^ evenZ ? grid.gridColorA[i] : grid.gridColorB[i]));
        }
    };
}

// returns number of bytes written
u32 ForEachPixelInRegion(const TextureRegion& region, std::span<byte> regionData, const PixelApplicator& generator)
{
    auto it = regionData.begin();
    auto firstWritten = it;
    for (u32 z = 0; z < region.extent.depth; z++)
    {
        for (u32 y = 0; y < region.extent.height; ++y)
        {
            for (u32 x = 0; x < region.extent.width; ++x)
            {
                generator(region, x, y, z, std::span<byte, 4>{ it, it + 4 });
                std::advance(it, 4);
            }
        }
    }

    return std::distance(firstWritten, it);
}

// returns number of bytes written
u32 ForEachPixelInRegions(std::span<const TextureRegion> regions,
                          std::span<byte> regionData,
                          const PixelApplicator& generator)
{
    auto begin = regionData.begin();
    auto firstWritten = begin;
    for (const auto& region : regions)
    {
        u32 writeCount = ForEachPixelInRegion(region, { begin, regionData.end() }, generator);
        std::advance(begin, writeCount);
    }
    return std::distance(firstWritten, begin);
}

void ValidateGridRegions(std::span<const TextureRegion> regions, std::span<byte> regionData)
{
    ForEachPixelInRegions(regions, regionData, ValidateGrid(DefaultGridParams));
}

SyncToken UploadTestGridToTexture(GfxBackend& graphics, const Texture& texture, std::span<const TextureRegion> regions)
{
    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(texture.description, regions));

    ForEachPixelInRegions(regions, fullImageData, GenerateGrid(DefaultGridParams));

    CommandContext ctx = graphics.BeginScopedCommandContext(CommandQueueType::Graphics, SubmissionPolicy::Immediate);

    ctx.EnqueueDataUpload(texture, std::as_bytes(std::span{ fullImageData }), regions);

    return *ctx.Submit();
}

std::vector<byte> ReadbackTextureContent(GfxBackend& graphics,
                                         const Texture& texture,
                                         std::span<const TextureRegion> regions,
                                         SyncToken token)
{
    CommandContext ctx = graphics.BeginScopedCommandContext(CommandQueueType::Graphics,
                                                            SubmissionPolicy::Immediate,
                                                            std::span{ &token, 1 });

    TextureReadbackContext readbackCtx = ctx.EnqueueDataReadback(texture, regions);
    graphics.WaitForTokenOnCPU(*ctx.Submit());

    std::vector<byte> fullImageData;
    fullImageData.resize(readbackCtx.GetByteDataSize());
    readbackCtx.ReadData(fullImageData);

    return fullImageData;
}

void RunTestsFor2DTextureSize(GfxBackend& graphics, u32 width, u32 height, u32& testId)
{
    static const TextureDescription textureDesc_1mip = { .name = std::format("{}x{}_1mip", width, height),
                                                         .type = TextureType::Texture2D,
                                                         .format = TextureFormat::RGBA8_UNORM,
                                                         .width = width,
                                                         .height = height,
                                                         .depthOrArraySize = 1,
                                                         .mips = 1,
                                                         .usage =
                                                             TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };
    static const std::vector<TextureRegion> regions_1mip = TextureRegion::AllMips(textureDesc_1mip);

    static const TextureDescription textureDesc_2mip = { .name = std::format("{}x{}_2mip", width, height),
                                                         .type = TextureType::Texture2D,
                                                         .format = TextureFormat::RGBA8_UNORM,
                                                         .width = width,
                                                         .height = height,
                                                         .depthOrArraySize = 1,
                                                         .mips = 2,
                                                         .usage =
                                                             TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };
    static const std::vector<TextureRegion> regions_2mip = TextureRegion::AllMips(textureDesc_2mip);
    static const std::vector<TextureRegion> regions_2mip_mip0 = TextureRegion::FullMip(0, textureDesc_2mip);
    static const std::vector<TextureRegion> regions_2mip_mip1 = TextureRegion::FullMip(1, textureDesc_2mip);

    {
        VEX_LOG(Info, "Test {}: {}x{} Full texture upload, 1 mip", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_1mip, ResourceLifetime::Static);

        UploadTestGridToTexture(graphics, texture, regions_1mip);

        graphics.DestroyTexture(texture);
    }

    {
        VEX_LOG(Info, "Test {}: {}x{} Full texture upload, 2 mips", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

        UploadTestGridToTexture(graphics, texture, regions_2mip);

        graphics.DestroyTexture(texture);
    }

    {
        VEX_LOG(Info, "Test {}: {}x{} Separate mip upload, 2 mips", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

        UploadTestGridToTexture(graphics, texture, regions_2mip_mip0);
        UploadTestGridToTexture(graphics, texture, regions_2mip_mip1);

        graphics.DestroyTexture(texture);
    }

    {
        VEX_LOG(Info, "Test {}: {}x{} upload/readback full texture, 1 mip", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_1mip, ResourceLifetime::Static);

        SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, regions_1mip);

        std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, regions_1mip, uploadToken);

        ValidateGridRegions(regions_1mip, textureData);

        graphics.DestroyTexture(texture);
    }

    {
        VEX_LOG(Info, "Test {}: {}x{} upload/readback full texture, 2 mip", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

        SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, regions_2mip);

        std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, regions_2mip, uploadToken);

        ValidateGridRegions(regions_2mip, textureData);

        graphics.DestroyTexture(texture);
    }

    {
        VEX_LOG(Info, "Test {}: {}x{} upload/readback separate mips, 2 mip", testId++, width, height);

        Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

        SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, regions_2mip);

        {
            std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, regions_2mip_mip0, uploadToken);
            ValidateGridRegions(regions_2mip_mip0, textureData);
        }

        {
            std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, regions_2mip_mip1, uploadToken);
            ValidateGridRegions(regions_2mip_mip1, textureData);
        }

        graphics.DestroyTexture(texture);
    }
}

void RunMiscTests(GfxBackend& graphics, CommandQueueType type, u32& testId)
{
    PixelApplicator cubemapApplicator = [](const TextureRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
    {
        switch (region.mip)
        {
        case 0:
            pixel[0] = static_cast<byte>(32 * region.slice);
            pixel[1] = byte{ 64 };
            pixel[2] = byte{ 128 };
            pixel[3] = byte{ 255 };
            break;
        case 1:
            pixel[0] = static_cast<byte>((region.slice % 2 == 0) * 255);
            pixel[1] = byte{ 0 };
            pixel[2] = byte{ 0 };
            pixel[3] = byte{ 255 };
        case 2:
            pixel[0] = byte{ 255 };
            pixel[1] = static_cast<byte>((region.slice % 2 == 0) * 255);
            pixel[2] = static_cast<byte>((region.slice % 2 != 0) * 255);
            pixel[3] = byte{ 255 };
        default:
            pixel[0] = byte{ 17 };
            pixel[1] = byte{ 17 };
            pixel[2] = byte{ 17 };
            pixel[3] = byte{ 17 };
            break;
        }
    };

    VEX_LOG(Info, "Test {}: Upload a cubemap with two mips", testId++);
    {
        auto ctx = graphics.BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        TextureDescription cubemapDesc =
            TextureDescription::CreateTextureCubeDesc("Cubemap", TextureFormat::RGBA8_UNORM, 16, 2);

        Texture cubemapTexture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

        std::vector<TextureRegion> regions = TextureRegion::AllMips(cubemapTexture.description);

        std::vector<byte> fullImageData;
        fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, regions));

        ForEachPixelInRegions(regions, fullImageData, cubemapApplicator);

        ctx.EnqueueDataUpload(cubemapTexture, std::as_bytes(std::span(fullImageData)), regions);

        ctx.Submit();

        graphics.DestroyTexture(cubemapTexture);
    }

    VEX_LOG(Info, "Test {}: Upload a 2d texture array of size 2 with 3 mips", testId++);
    {
        auto ctx = graphics.BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        TextureDescription cubemapDesc =
            TextureDescription::CreateTexture2DArrayDesc("2dTextureArray", TextureFormat::RGBA8_UNORM, 16, 12, 2, 3);

        std::vector<TextureRegion> regions = TextureRegion::AllMips(cubemapDesc);

        Texture texture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

        std::vector<byte> fullImageData;
        fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, regions));

        ForEachPixelInRegions(regions, fullImageData, cubemapApplicator);

        ctx.EnqueueDataUpload(texture, std::as_bytes(std::span(fullImageData)), regions);

        ctx.Submit();

        graphics.DestroyTexture(texture);
    }

    VEX_LOG(Info, "Test {}: Upload a texture cube array of size 3 with 2 mips", testId++);
    {
        auto ctx = graphics.BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        TextureDescription cubemapDesc =
            TextureDescription::CreateTextureCubeArrayDesc("CubemapArray", TextureFormat::RGBA8_UNORM, 16, 3, 2);

        std::vector<TextureRegion> regions = TextureRegion::AllMips(cubemapDesc);

        Texture cubemapTexture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

        std::vector<byte> fullImageData;
        fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, regions));

        ForEachPixelInRegions(regions, fullImageData, cubemapApplicator);

        ctx.EnqueueDataUpload(cubemapTexture, std::as_bytes(std::span(fullImageData)), regions);

        ctx.Submit();

        graphics.DestroyTexture(cubemapTexture);
    }

    VEX_LOG(Info, "Test {}: Upload a 3d texture of depth 2 with 3 mips", testId++);
    {
        auto ctx = graphics.BeginScopedCommandContext(type, SubmissionPolicy::Immediate);

        // Cursed non-even sizes.
        TextureDescription cubemapDesc =
            TextureDescription::CreateTexture3DDesc("3DTexture", TextureFormat::RGBA8_UNORM, 121, 165, 64, 3);

        std::vector<TextureRegion> regions = TextureRegion::AllMips(cubemapDesc);

        Texture texture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

        std::vector<byte> fullImageData;
        fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, regions));

        ForEachPixelInRegions(regions, fullImageData, cubemapApplicator);

        ctx.EnqueueDataUpload(texture, std::as_bytes(std::span(fullImageData)), regions);

        ctx.Submit();

        graphics.DestroyTexture(texture);
    }
}

} // namespace TextureTests

void TextureUploadDownladTests(GfxBackend& graphics)
{
    VEX_LOG(Info, "---- Starting Texture Upload/Readback Test... ----");

    u32 testId = 1;

    TextureTests::RunTestsFor2DTextureSize(graphics, 256, 256, testId);
    TextureTests::RunTestsFor2DTextureSize(graphics, 546, 627, testId);
    TextureTests::RunMiscTests(graphics, CommandQueueType::Graphics, testId);

    graphics.FlushGPU();
}

} // namespace vex
