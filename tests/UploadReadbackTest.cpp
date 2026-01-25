#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex/CommandContext.h>
#include <Vex/Graphics.h>
#include <Vex/Logger.h>
#include <Vex/Types.h>

namespace vex
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
u64 ForEachPixelInRegion(const TextureDesc& desc,
                         const TextureRegion& region,
                         std::span<byte> regionData,
                         const PixelApplicator& generator)
{
    auto it = regionData.begin();
    auto firstWritten = it;

    for (u16 mip = region.subresource.startMip;
         mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
         ++mip)
    {
        TextureRegion currRegion = region;
        currRegion.subresource.startMip = mip;
        currRegion.subresource.mipCount = 1;
        for (u32 z = 0; z < region.extent.GetDepth(desc, mip); z++)
        {
            for (u32 y = 0; y < region.extent.GetHeight(desc, mip); ++y)
            {
                for (u32 x = 0; x < region.extent.GetWidth(desc, mip); ++x)
                {
                    generator(currRegion, x, y, z, std::span<byte, 4>{ it, it + 4 });
                    std::advance(it, 4);
                }
            }
        }
    }

    return std::distance(firstWritten, it);
}

// returns number of bytes written
u64 ForEachPixelInRegions(const TextureDesc& desc,
                          std::span<const TextureRegion> regions,
                          std::span<byte> regionData,
                          const PixelApplicator& generator)
{
    auto begin = regionData.begin();
    auto firstWritten = begin;
    for (const auto& region : regions)
    {
        u64 writeCount = ForEachPixelInRegion(desc, region, { begin, regionData.end() }, generator);
        std::advance(begin, writeCount);
    }
    return std::distance(firstWritten, begin);
}

void ValidateGridRegions(const TextureDesc& desc, std::span<const TextureRegion> regions, std::span<byte> regionData)
{
    ForEachPixelInRegions(desc, regions, regionData, ValidateGrid(DefaultGridParams));
}

SyncToken UploadTestGridToTexture(Graphics& graphics, const Texture& texture, std::span<const TextureRegion> regions)
{
    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(texture.desc, regions));

    ForEachPixelInRegions(texture.desc, regions, fullImageData, GenerateGrid(DefaultGridParams));

    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    ctx.EnqueueDataUpload(texture, std::as_bytes(std::span{ fullImageData }), regions);

    return graphics.Submit(ctx);
}

std::vector<byte> ReadbackTextureContent(Graphics& graphics,
                                         const Texture& texture,
                                         std::span<const TextureRegion> regions,
                                         SyncToken token)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Graphics);

    TextureReadbackContext readbackCtx = ctx.EnqueueDataReadback(texture, regions);
    graphics.WaitForTokenOnCPU(graphics.Submit(ctx, std::span{ &token, 1 }));

    std::vector<byte> fullImageData(readbackCtx.GetDataByteSize());
    readbackCtx.ReadData(fullImageData);

    return fullImageData;
}

struct Texture2DTestParam
{
    u32 width{};
    u32 height{};
};

class FixedSizeTexture2DTest : public VexTestParam<Texture2DTestParam>
{
public:
    TextureDesc textureDesc_1mip{};
    TextureRegion regions_1mip;

    TextureDesc textureDesc_2mip{};
    TextureRegion regions_2mip;
    TextureRegion regions_2mip_mip0;
    TextureRegion regions_2mip_mip1;

    void SetUp() override
    {
        u32 width = GetParam().width;
        u32 height = GetParam().height;

        textureDesc_1mip = { .name = std::format("{}x{}_1mip", width, height),
                             .type = TextureType::Texture2D,
                             .format = TextureFormat::RGBA8_UNORM,
                             .width = width,
                             .height = height,
                             .depthOrSliceCount = 1,
                             .mips = 1,
                             .usage = TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };

        regions_1mip = TextureRegion::AllMips();

        textureDesc_2mip = { .name = std::format("{}x{}_2mip", width, height),
                             .type = TextureType::Texture2D,
                             .format = TextureFormat::RGBA8_UNORM,
                             .width = width,
                             .height = height,
                             .depthOrSliceCount = 1,
                             .mips = 2,
                             .usage = TextureUsage::ShaderRead | TextureUsage::ShaderReadWrite };

        regions_2mip = TextureRegion::AllMips();
        regions_2mip_mip0 = TextureRegion::SingleMip(0);
        regions_2mip_mip1 = TextureRegion::SingleMip(1);
    }
};

TEST_P(FixedSizeTexture2DTest, FullTextureUpload1Mip)
{
    Texture texture = graphics.CreateTexture(textureDesc_1mip, ResourceLifetime::Static);

    UploadTestGridToTexture(graphics, texture, { &regions_1mip, 1 });

    graphics.DestroyTexture(texture);
}

TEST_P(FixedSizeTexture2DTest, FullTextureUpload2Mips)
{
    Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

    UploadTestGridToTexture(graphics, texture, { &regions_2mip, 1 });

    graphics.DestroyTexture(texture);
}

TEST_P(FixedSizeTexture2DTest, SeparateMipUpload2Mips)
{
    Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

    UploadTestGridToTexture(graphics, texture, { &regions_2mip_mip0, 1 });
    UploadTestGridToTexture(graphics, texture, { &regions_2mip_mip1, 1 });

    graphics.DestroyTexture(texture);
}

TEST_P(FixedSizeTexture2DTest, UploadReadbackFull1Mip)
{
    Texture texture = graphics.CreateTexture(textureDesc_1mip, ResourceLifetime::Static);

    SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, { &regions_1mip, 1 });

    std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, { &regions_1mip, 1 }, uploadToken);

    ValidateGridRegions(texture.desc, { &regions_1mip, 1 }, textureData);

    graphics.DestroyTexture(texture);
}

TEST_P(FixedSizeTexture2DTest, UploadReadbackFull2Mips)
{
    Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

    SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, { &regions_2mip, 1 });

    std::vector<byte> textureData = ReadbackTextureContent(graphics, texture, { &regions_2mip, 1 }, uploadToken);

    ValidateGridRegions(texture.desc, { &regions_2mip, 1 }, textureData);

    graphics.DestroyTexture(texture);
}

TEST_P(FixedSizeTexture2DTest, UploadFullReadbackSeparate2Mips)
{
    Texture texture = graphics.CreateTexture(textureDesc_2mip, ResourceLifetime::Static);

    SyncToken uploadToken = UploadTestGridToTexture(graphics, texture, { &regions_2mip, 1 });

    {
        std::vector<byte> textureData =
            ReadbackTextureContent(graphics, texture, { &regions_2mip_mip0, 1 }, uploadToken);
        ValidateGridRegions(texture.desc, { &regions_2mip_mip0, 1 }, textureData);
    }

    {
        std::vector<byte> textureData =
            ReadbackTextureContent(graphics, texture, { &regions_2mip_mip1, 1 }, uploadToken);
        ValidateGridRegions(texture.desc, { &regions_2mip_mip1, 1 }, textureData);
    }

    graphics.DestroyTexture(texture);
}

struct MiscTextureTests : public VexTestParam<QueueType>
{
    PixelApplicator cubemapApplicator = [](const TextureRegion& region, u32 x, u32 y, u32 z, std::span<byte, 4> pixel)
    {
        switch (region.subresource.startMip)
        {
        case 0:
            pixel[0] = static_cast<byte>(32 * region.subresource.startSlice);
            pixel[1] = byte{ 64 };
            pixel[2] = byte{ 128 };
            pixel[3] = byte{ 255 };
            break;
        case 1:
            pixel[0] = static_cast<byte>((region.subresource.startSlice % 2 == 0) * 255);
            pixel[1] = byte{ 0 };
            pixel[2] = byte{ 0 };
            pixel[3] = byte{ 255 };
            break;
        case 2:
            pixel[0] = byte{ 255 };
            pixel[1] = static_cast<byte>((region.subresource.startSlice % 2 == 0) * 255);
            pixel[2] = static_cast<byte>((region.subresource.startSlice % 2 != 0) * 255);
            pixel[3] = byte{ 255 };
            break;
        default:
            pixel[0] = byte{ 17 };
            pixel[1] = byte{ 17 };
            pixel[2] = byte{ 17 };
            pixel[3] = byte{ 17 };
            break;
        }
    };
};

TEST_P(MiscTextureTests, FullUploadCubemap2Mips)
{
    CommandContext ctx = graphics.CreateCommandContext(GetParam());

    TextureDesc cubemapDesc = TextureDesc::CreateTextureCubeDesc("Cubemap", TextureFormat::RGBA8_UNORM, 16, 2);

    Texture cubemapTexture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

    TextureRegion regions = TextureRegion::AllMips();

    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, { &regions, 1 }));

    ForEachPixelInRegions(cubemapTexture.desc, { &regions, 1 }, fullImageData, cubemapApplicator);

    ctx.EnqueueDataUpload(cubemapTexture, std::as_bytes(std::span(fullImageData)), regions);

    graphics.Submit(ctx);

    graphics.DestroyTexture(cubemapTexture);
}

TEST_P(MiscTextureTests, FullUpload2DTexture2Slices3Mips)
{
    CommandContext ctx = graphics.CreateCommandContext(GetParam());

    TextureDesc cubemapDesc =
        TextureDesc::CreateTexture2DArrayDesc("2dTextureArray", TextureFormat::RGBA8_UNORM, 16, 12, 2, 3);

    TextureRegion regions = TextureRegion::AllMips();

    Texture texture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, { &regions, 1 }));

    ForEachPixelInRegions(texture.desc, { &regions, 1 }, fullImageData, cubemapApplicator);

    ctx.EnqueueDataUpload(texture, std::as_bytes(std::span(fullImageData)), regions);

    graphics.Submit(ctx);

    graphics.DestroyTexture(texture);
}

TEST_P(MiscTextureTests, FullUploadTextureCube3Slices2Mips)
{
    CommandContext ctx = graphics.CreateCommandContext(GetParam());

    TextureDesc cubemapDesc =
        TextureDesc::CreateTextureCubeArrayDesc("CubemapArray", TextureFormat::RGBA8_UNORM, 16, 3, 2);

    TextureRegion regions = TextureRegion::AllMips();

    Texture cubemapTexture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, { &regions, 1 }));

    ForEachPixelInRegions(cubemapTexture.desc, { &regions, 1 }, fullImageData, cubemapApplicator);

    ctx.EnqueueDataUpload(cubemapTexture, std::as_bytes(std::span(fullImageData)), regions);

    graphics.Submit(ctx);

    graphics.DestroyTexture(cubemapTexture);
}

TEST_P(MiscTextureTests, FullUpload3DTexture3Mips)
{
    CommandContext ctx = graphics.CreateCommandContext(GetParam());

    // Cursed non-even sizes.
    TextureDesc cubemapDesc =
        TextureDesc::CreateTexture3DDesc("3DTexture", TextureFormat::RGBA8_UNORM, 121, 165, 64, 3);

    TextureRegion regions = TextureRegion::AllMips();

    Texture texture = graphics.CreateTexture(cubemapDesc, ResourceLifetime::Static);

    std::vector<byte> fullImageData;
    fullImageData.resize(TextureUtil::ComputePackedTextureDataByteSize(cubemapDesc, { &regions, 1 }));

    ForEachPixelInRegions(texture.desc, { &regions, 1 }, fullImageData, cubemapApplicator);

    ctx.EnqueueDataUpload(texture, std::as_bytes(std::span(fullImageData)), regions);

    graphics.Submit(ctx);

    graphics.DestroyTexture(texture);
}

INSTANTIATE_TEST_SUITE_P(PerQueueType, MiscTextureTests, QueueTypeValue);

struct BufferUploadReadbackTestParams
{
    BufferRegion uploadRegion = BufferRegion::FullBuffer();
    BufferRegion readbackRegion = BufferRegion::FullBuffer();
};

struct BufferUploadReadbackTests : public VexPerQueueTestWithParam<BufferUploadReadbackTestParams>
{
};

TEST_P(BufferUploadReadbackTests, BufferUploadAndFullReadback)
{
    CommandContext ctx = graphics.CreateCommandContext(GetQueueType());

    BufferUploadReadbackTestParams params = GetParam();

    static constexpr auto N = 100;

    float data[N];
    for (int i = 0; i < N; ++i)
    {
        data[i] = static_cast<float>(i);
    }

    Buffer uploadBuffer =
        graphics.CreateBuffer(BufferDesc::CreateGenericBufferDesc("GPUBuffer", sizeof(float) * 100));

    ctx.EnqueueDataUpload(uploadBuffer, std::as_bytes(std::span(data)), params.uploadRegion);

    auto readbackContext = ctx.EnqueueDataReadback(uploadBuffer, params.readbackRegion);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    float readback[N];
    auto readbackFloatCount = params.readbackRegion.byteSize / sizeof(float);
    auto readbackFloatOffset = (params.readbackRegion.offset - params.uploadRegion.offset) / sizeof(float);
    readbackContext.ReadData(std::as_writable_bytes(std::span{ readback, readbackFloatCount }));

    for (int i = 0; i < readbackFloatCount; ++i)
    {
        ASSERT_TRUE(data[readbackFloatOffset + i] == readback[i]);
    }
}

INSTANTIATE_TEST_SUITE_P(VariousSizes,
                         FixedSizeTexture2DTest,
                         testing::Values(Texture2DTestParam{ 256, 256 }, Texture2DTestParam{ 546, 627 }));

INSTANTIATE_PER_QUEUE_TEST_SUITE_P(
    PerQueueType,
    BufferUploadReadbackTests,
    testing::Values(
        // Upload full buffer and read the first 50 floats
        BufferUploadReadbackTestParams{ .readbackRegion = { .byteSize = sizeof(float) * 50 } },
        // Upload full buffer and read the last 50 floats
        BufferUploadReadbackTestParams{
            .readbackRegion = { .offset = sizeof(float) * 50, .byteSize = sizeof(float) * 50 } },
        // Upload only first 50 floats and read only the first 50 floats
        BufferUploadReadbackTestParams{ .uploadRegion = { .byteSize = sizeof(float) * 50 },
                                        .readbackRegion = { .byteSize = sizeof(float) * 50 } },
        // Upload only last 50 floats and read only the last 50 floats
        BufferUploadReadbackTestParams{
            .uploadRegion = { .offset = sizeof(float) * 50, .byteSize = sizeof(float) * 50 },
            .readbackRegion = { .offset = sizeof(float) * 50, .byteSize = sizeof(float) * 50 } },
        // upload 50 floats from the 23rd float and readback 10 floats from the 32nd
        BufferUploadReadbackTestParams{
            .uploadRegion = { .offset = sizeof(float) * 23, .byteSize = sizeof(float) * 50 },
            .readbackRegion = { .offset = sizeof(float) * 32, .byteSize = sizeof(float) * 10 } }));

struct ScalarBlockLayoutTests : public VexTestParam<ShaderCompilerBackend>
{
    struct WeirdlyPackedData
    {
        float vector1[3];
        float vector2[3];
        float vector3[3];
    };
};

TEST_P(ScalarBlockLayoutTests, ComputeMisalignedData)
{
    CommandContext ctx = graphics.CreateCommandContext(QueueType::Compute);

    std::array<WeirdlyPackedData, 13> data;
    for (int i = 0; i < 13; ++i)
    {
        int v1 = 1, v2 = 4, v3 = 7;
        for (int j = 0; j < 3; ++j)
        {
            data[i].vector1[j] = v1++;
            data[i].vector2[j] = v2++;
            data[i].vector3[j] = v3++;
        }
    }

    Buffer dataBuffer =
        graphics.CreateBuffer(BufferDesc{ .name = "DataBuffer", .byteSize = data.size() * sizeof(WeirdlyPackedData) });
    Buffer resultBuffer =
        graphics.CreateBuffer(BufferDesc{ .name = "ResultBuffer",
                                          .byteSize = 3 * sizeof(float),
                                          .usage = BufferUsage::GenericBuffer | BufferUsage::ReadWriteBuffer });

    ctx.EnqueueDataUpload(dataBuffer, std::as_bytes(std::span{ data }));

    ResourceBinding bindings[]{
        BufferBinding{ .buffer = dataBuffer,
                       .usage = BufferBindingUsage::StructuredBuffer,
                       .strideByteSize = static_cast<u32>(sizeof(WeirdlyPackedData)) },
        BufferBinding{
            .buffer = resultBuffer,
            .usage = BufferBindingUsage::RWStructuredBuffer,
            .strideByteSize = static_cast<u32>(3 * sizeof(float)),
        },
    };
    std::vector<BindlessHandle> handles = graphics.GetBindlessHandles(bindings);

    ctx.BarrierBindings(bindings);

    ctx.Dispatch(
        {
            .path = GetParam() == ShaderCompilerBackend::DXC
                        ? VexRootPath / "tests/shaders/ScalarBlockLayoutTest.hlsl"
                        : VexRootPath / "tests/shaders/ScalarBlockLayoutTest.slang",
            .entryPoint = "CSMain",
            .type = ShaderType::ComputeShader,
        },
        ConstantBinding(std::span(handles)),
        {
            1u,
            1u,
            1u,
        });

    BufferReadbackContext readbackContext = ctx.EnqueueDataReadback(resultBuffer);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    float result[3]{};
    readbackContext.ReadData(std::as_writable_bytes(std::span{ result }));

    EXPECT_TRUE(result[0] == 13 * (1 + 4 + 7));
    EXPECT_TRUE(result[1] == 13 * (2 + 5 + 8));
    EXPECT_TRUE(result[2] == 13 * (3 + 6 + 9));
}

INSTANTIATE_TEST_SUITE_P(PerShaderCompiler,
                         ScalarBlockLayoutTests,
                         testing::Values(ShaderCompilerBackend::DXC, ShaderCompilerBackend::Slang));

} // namespace vex
