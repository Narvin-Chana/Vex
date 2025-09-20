#include "TextureUploadTests.h"

#include <Vex/CommandContext.h>
#include <Vex/GfxBackend.h>
#include <Vex/Logger.h>
#include <Vex/Types.h>

namespace vex
{

using PixelApplicator =
    std::function<void(const TextureUploadRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)>;

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
    return [=](const TextureUploadRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
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
    return [=](const TextureUploadRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
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
u32 ForEachPixelInRegion(const TextureUploadRegion& region,
                         std::span<byte> regionData,
                         const PixelApplicator& generator)
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
u32 ForEachPixelInRegions(std::span<const TextureUploadRegion> regions,
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

void ValidateGridRegions(std::span<const TextureUploadRegion> regions, std::span<byte> regionData)
{
    ForEachPixelInRegions(regions, regionData, ValidateGrid(DefaultGridParams));
}

SyncToken UploadTestGridToTexture(GfxBackend& graphics,
                                  const Texture& texture,
                                  std::span<const TextureUploadRegion> regions)
{
    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedUploadDataByteSize(texture.description, regions));

    ForEachPixelInRegions(regions, fullImageData, GenerateGrid(DefaultGridParams));

    CommandContext ctx = graphics.BeginScopedCommandContext(CommandQueueType::Graphics, SubmissionPolicy::Immediate);

    ctx.EnqueueDataUpload(texture, std::as_bytes(std::span{ fullImageData }), regions);

    return *ctx.Submit();
}

std::vector<byte> ReadbackTextureContent(GfxBackend& graphics,
                                         const Texture& texture,
                                         std::span<const TextureUploadRegion> regions,
                                         SyncToken token)
{
    CommandContext ctx = graphics.BeginScopedCommandContext(CommandQueueType::Graphics,
                                                            SubmissionPolicy::Immediate,
                                                            std::span{ &token, 1 });

    ReadBackContext readbackCtx = ctx.EnqueueDataReadback(texture, regions);
    SyncToken readbackToken = *ctx.Submit();

    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedUploadDataByteSize(texture.description, regions));
    graphics.ExecuteReadback(readbackCtx, fullImageData, readbackToken);

    return fullImageData;
}

void RunTestsForTextureSize(GfxBackend& graphics, u32 width, u32 height, u32& testId)
{
    static const TextureDescription textureDesc_1mip = { .name = std::format("{}x{}_1mip", width, height),
                                                         .type = TextureType::Texture2D,
                                                         .width = width,
                                                         .height = height,
                                                         .depthOrArraySize = 1,
                                                         .mips = 1,
                                                         .format = TextureFormat::RGBA8_UNORM,
                                                         .usage =
                                                             TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };
    static const std::vector<TextureUploadRegion> regions_1mip = TextureUploadRegion::UploadAllMips(textureDesc_1mip);

    static const TextureDescription textureDesc_2mip = { .name = std::format("{}x{}_2mip", width, height),
                                                         .type = TextureType::Texture2D,
                                                         .width = width,
                                                         .height = height,
                                                         .depthOrArraySize = 1,
                                                         .mips = 2,
                                                         .format = TextureFormat::RGBA8_UNORM,
                                                         .usage =
                                                             TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };
    static const std::vector<TextureUploadRegion> regions_2mip = TextureUploadRegion::UploadAllMips(textureDesc_2mip);
    static const std::vector<TextureUploadRegion> regions_2mip_mip0 =
        TextureUploadRegion::UploadFullMip(0, textureDesc_2mip);
    static const std::vector<TextureUploadRegion> regions_2mip_mip1 =
        TextureUploadRegion::UploadFullMip(1, textureDesc_2mip);

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

void TextureUploadDownladTests(GfxBackend& graphics)
{
    VEX_LOG(Info, "---- Starting Texture Upload/Readback Test... ----");

    u32 testId = 1;

    RunTestsForTextureSize(graphics, 256, 256, testId);
    RunTestsForTextureSize(graphics, 546, 627, testId);
}

} // namespace vex
