#include "VexTest.h"

#include <gtest/gtest.h>

using namespace vex;

// Disabled for vulkan since RayTracing is not yet implemented.
#if VEX_DX12

struct AccelerationStructureTest : VexTest
{
    using Vertex = std::array<float, 3>;

    Buffer triangleVertexBuffer;
    Buffer triangleIndexBuffer;

    void SetUp() override
    {
        auto ctx = graphics.CreateCommandContext(QueueType::Compute);

        const BufferDesc vbDesc =
            BufferDesc::CreateVertexBufferDesc("RT Triangle Vertex Buffer", sizeof(Vertex) * TriangleVerts.size());
        triangleVertexBuffer = graphics.CreateBuffer(vbDesc);

        const BufferDesc ibDesc =
            BufferDesc::CreateIndexBufferDesc("RT Triangle Index Buffer", sizeof(u32) * TriangleIndices.size());
        triangleIndexBuffer = graphics.CreateBuffer(ibDesc);

        ctx.EnqueueDataUpload(triangleVertexBuffer, std::as_bytes(std::span(TriangleVerts)));
        ctx.EnqueueDataUpload(triangleIndexBuffer, std::as_bytes(std::span(TriangleIndices)));

        graphics.Submit(ctx);

        VexTest::SetUp();
    }

    void TearDown() override
    {
        VexTest::TearDown();
        graphics.DestroyBuffer(triangleVertexBuffer);
        graphics.DestroyBuffer(triangleIndexBuffer);
    }

private:
    static constexpr float DepthValue = 1.0;
    static constexpr float Offset = 0.7f;
    static constexpr std::array TriangleVerts{
        // Triangle
        Vertex{ 0, Offset, DepthValue },
        Vertex{ Offset, -Offset, DepthValue },
        Vertex{ -Offset, -Offset, DepthValue },
    };
    static constexpr std::array<u32, 3> TriangleIndices{ 0, 1, 2 };
};

///////////////////////////
// BLAS Tests
///////////////////////////

TEST_F(AccelerationStructureTest, CreateSimpleTriangleBLAS_Vertex)
{
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    ctx.BuildBLAS(blas,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .transform = std::nullopt,
                        .flags = ASGeometry::Opaque,
                    } } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
}

TEST_F(AccelerationStructureTest, CreateSimpleTriangleBLAS_VertexAndIndex)
{
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    ctx.BuildBLAS(blas,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = ASGeometry::Opaque,
                    } } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
}

TEST_F(AccelerationStructureTest, CreateMultipleTriangleBLAS_VertexAndIndex_Transforms)
{
    auto blas1 = graphics.CreateAccelerationStructure(ASDesc{ .name = "BLAS1", .type = ASType::BottomLevel });
    auto blas2 = graphics.CreateAccelerationStructure(ASDesc{ .name = "BLAS2", .type = ASType::BottomLevel });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    ctx.BuildBLAS(blas1,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = {{ 
                                1, 0, 0, 1,
                                0, 1, 0, 5,
                                0, 0, 1, -10,
                        }},
                        .flags = ASGeometry::Opaque,
                    } } });
    ctx.BuildBLAS(blas2,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = {{ 
                                1, 0, 0, 10,
                                0, 1, 0, -5,
                                0, 0, 1, 1,
                        }},
                        .flags = ASGeometry::Opaque,
                    } } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas1);
    graphics.DestroyAccelerationStructure(blas2);
}

struct BLASFlagTestData
{
    ASGeometry::Flags geometryFlags;
    ASBuild::Flags buildFlags;

    // Claude-generated helper for test naming
    std::string ToString() const
    {
        std::string name;

        // Geometry flags
        if (geometryFlags == ASGeometry::None)
            name += "GeomNone";
        else
        {
            name += "Geom";
            if (geometryFlags & ASGeometry::Opaque)
                name += "Opaque";
            if (geometryFlags & ASGeometry::NoDuplicateAnyHitInvocation)
                name += "NoDupAnyHit";
        }

        name += "_";

        // Build flags
        if (buildFlags == ASBuild::None)
            name += "BuildNone";
        else
        {
            name += "Build";
            if (buildFlags & ASBuild::AllowUpdate)
                name += "AllowUpd";
            if (buildFlags & ASBuild::AllowCompaction)
                name += "AllowComp";
            if (buildFlags & ASBuild::PreferFastTrace)
                name += "FastTrace";
            if (buildFlags & ASBuild::PreferFastBuild)
                name += "FastBuild";
            if (buildFlags & ASBuild::MinimizeMemory)
                name += "MinMem";
            if (buildFlags & ASBuild::PerformUpdate)
                name += "PerfUpd";
        }

        return name;
    }
};

static std::vector<BLASFlagTestData> GenerateTestCasesForBLASFlagTest()
{
    return {
        // Basic cases
        { ASGeometry::None, ASBuild::None },
        { ASGeometry::Opaque, ASBuild::None },
        // Performance variants
        { ASGeometry::Opaque, ASBuild::PreferFastTrace },
        { ASGeometry::Opaque, ASBuild::PreferFastBuild },
        // Update
        { ASGeometry::Opaque, ASBuild::AllowUpdate },
        // TODO(https://trello.com/c/LIEtASpP): Disabled until AS update is implemented!
        // { ASGeometry::Opaque, ASBuild::AllowUpdate | ASBuild::PerformUpdate },
        // Compaction
        { ASGeometry::Opaque, ASBuild::AllowCompaction },
        { ASGeometry::Opaque, ASBuild::AllowCompaction | ASBuild::PreferFastTrace },
        // All flags combined (where valid)
        { ASGeometry::Opaque | ASGeometry::NoDuplicateAnyHitInvocation,
          ASBuild::AllowUpdate | ASBuild::AllowCompaction | ASBuild::PreferFastTrace | ASBuild::MinimizeMemory },
    };
}

struct BLASFlagTest
    : public AccelerationStructureTest
    , public testing::WithParamInterface<BLASFlagTestData>
{
};

TEST_P(BLASFlagTest, BLASFlagPermutations)
{
    const auto& testData = GetParam();
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "BLAS", .type = ASType::BottomLevel, .buildFlags = testData.buildFlags });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    ctx.BuildBLAS(blas,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = testData.geometryFlags,
                    } } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
}

INSTANTIATE_TEST_SUITE_P(BLASFlagPermutationTest,
                         BLASFlagTest,
                         testing::ValuesIn(GenerateTestCasesForBLASFlagTest()),
                         [](const ::testing::TestParamInfo<BLASFlagTestData>& info) { return info.param.ToString(); });

///////////////////////////
// TLAS Tests
///////////////////////////

struct TLASAccelerationStructureTest : public AccelerationStructureTest
{
    AccelerationStructure triangleBLAS;

    void SetUp() override
    {
        AccelerationStructureTest::SetUp();
        triangleBLAS = graphics.CreateAccelerationStructure(
            ASDesc{ .name = "Triangle BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });

        auto ctx = graphics.CreateCommandContext(QueueType::Compute);
        ctx.BuildBLAS(triangleBLAS,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = ASGeometry::Opaque,
                    } } });
        graphics.Submit(ctx);
    }

    void TearDown() override
    {
        AccelerationStructureTest::TearDown();
        graphics.DestroyAccelerationStructure(triangleBLAS);
    }
};

TEST_F(TLASAccelerationStructureTest, CreateSimpleTriangleTLAS_Instance)
{
    auto tlas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "TLAS", .type = ASType::TopLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);

    TLASInstanceDesc instanceDesc{
        .transform = {
            1.0f, 0.0f, 0.0f, -0.3f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        },
        .instanceID = 0,
        .blas = triangleBLAS,
    };
    ctx.BuildTLAS(tlas, { .instances = { instanceDesc } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(tlas);
}

TEST_F(TLASAccelerationStructureTest, CreateSimpleTriangleTLAS_2Instances_InstanceMaskAndSBTContribution)
{
    auto tlas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "TLAS", .type = ASType::TopLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    std::array<TLASInstanceDesc, 2> instances{
        TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, -0.3f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            },
            .instanceID = 0,
            .instanceMask = 0xEF,
            .instanceContributionToHitGroupIndex = 3,
            .blas = triangleBLAS,
        },
        TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, 0.3f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f,
            },
            .instanceID = 34,
            .instanceMask = 0xF1,
            .instanceContributionToHitGroupIndex = 5,
            .blas = triangleBLAS,
        },
    };
    ctx.BuildTLAS(tlas, { .instances = instances });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(tlas);
}

struct TLASFlagTestData
{
    ASInstance::Flags instanceFlags;
    ASBuild::Flags buildFlags;

    // Another Claude-generated function to obtain a readable string from the selected flags.
    std::string ToString() const
    {
        std::string name;

        // Instance flags
        if (instanceFlags == ASInstance::None)
        {
            name += "InstNone";
        }
        else
        {
            name += "Inst";
            if (instanceFlags & ASInstance::TriangleCullDisable)
                name += "CullDisable";
            if (instanceFlags & ASInstance::TriangleFrontCounterClockwise)
                name += "CCW";
            if (instanceFlags & ASInstance::ForceOpaque)
                name += "ForceOpaque";
            if (instanceFlags & ASInstance::ForceNonOpaque)
                name += "ForceNonOpaque";
        }

        name += "_";

        // Build flags
        if (buildFlags == ASBuild::None)
        {
            name += "BuildNone";
        }
        else
        {
            name += "Build";
            if (buildFlags & ASBuild::AllowUpdate)
                name += "AllowUpd";
            if (buildFlags & ASBuild::AllowCompaction)
                name += "AllowComp";
            if (buildFlags & ASBuild::PreferFastTrace)
                name += "FastTrace";
            if (buildFlags & ASBuild::PreferFastBuild)
                name += "FastBuild";
            if (buildFlags & ASBuild::MinimizeMemory)
                name += "MinMem";
            if (buildFlags & ASBuild::PerformUpdate)
                name += "PerfUpd";
        }

        return name;
    }
};

std::vector<TLASFlagTestData> GenerateTestCasesForTLASFlagTest()
{
    return {
        // Basic cases
        { ASInstance::None, ASBuild::None },

        // Instance flag variations (with no build flags)
        { ASInstance::TriangleCullDisable, ASBuild::None },
        { ASInstance::TriangleFrontCounterClockwise, ASBuild::None },
        { ASInstance::ForceOpaque, ASBuild::None },
        { ASInstance::ForceNonOpaque, ASBuild::None },

        // Combined instance flags
        { ASInstance::TriangleCullDisable | ASInstance::TriangleFrontCounterClockwise, ASBuild::None },
        { ASInstance::TriangleCullDisable | ASInstance::ForceOpaque, ASBuild::None },
        { ASInstance::TriangleFrontCounterClockwise | ASInstance::ForceNonOpaque, ASBuild::None },

        // Build performance variants (with opaque instances)
        { ASInstance::ForceOpaque, ASBuild::PreferFastTrace },
        { ASInstance::ForceOpaque, ASBuild::PreferFastBuild },
        { ASInstance::ForceOpaque, ASBuild::MinimizeMemory },

        // Update paths
        { ASInstance::None, ASBuild::AllowUpdate },
        // TODO(https://trello.com/c/LIEtASpP): Disabled until AS update is implemented!
        //{ ASInstance::None, ASBuild::AllowUpdate | ASBuild::PerformUpdate },
        { ASInstance::ForceOpaque, ASBuild::AllowUpdate | ASBuild::PreferFastTrace },

        // Compaction
        { ASInstance::None, ASBuild::AllowCompaction },
        { ASInstance::ForceOpaque, ASBuild::AllowCompaction | ASBuild::PreferFastTrace },

        // Complex combinations
        { ASInstance::TriangleCullDisable | ASInstance::ForceOpaque,
          ASBuild::AllowUpdate | ASBuild::AllowCompaction | ASBuild::PreferFastTrace },

        { ASInstance::TriangleFrontCounterClockwise | ASInstance::ForceNonOpaque,
          ASBuild::AllowUpdate | ASBuild::MinimizeMemory },

        // Kitchen sink (all compatible flags)
        { ASInstance::TriangleCullDisable | ASInstance::TriangleFrontCounterClockwise | ASInstance::ForceOpaque,
          ASBuild::AllowUpdate | ASBuild::AllowCompaction | ASBuild::PreferFastTrace | ASBuild::MinimizeMemory },
    };
}

struct TLASFlagTest
    : public TLASAccelerationStructureTest
    , public testing::WithParamInterface<TLASFlagTestData>
{
};

TEST_P(TLASFlagTest, TLASFlagPermutations)
{
    const auto& testData = GetParam();
    auto tlas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "TLAS", .type = ASType::TopLevel, .buildFlags = testData.buildFlags });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    TLASInstanceDesc instanceDesc{
        .instanceFlags = testData.instanceFlags,
        .blas = triangleBLAS,
    };
    ctx.BuildTLAS(tlas, { .instances = { instanceDesc } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(tlas);
}

INSTANTIATE_TEST_SUITE_P(TLASFlagPermutationTest,
                         TLASFlagTest,
                         testing::ValuesIn(GenerateTestCasesForTLASFlagTest()),
                         [](const ::testing::TestParamInfo<TLASFlagTestData>& info) { return info.param.ToString(); });

///////////////////////////
// AABB AS Tests
///////////////////////////

TEST_F(AccelerationStructureTest, CreateSimpleAABBBLAS)
{
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    // Build BLAS with single AABB.
    ctx.BuildBLAS(blas,
                  { .type = ASGeometryType::AABBs,
                    .geometry = {
                        BLASGeometryDesc{
                            .aabbs = { AABB{ 0, 0, 0, 1, 1, 1 } },
                            .flags = ASGeometry::Opaque,
                        },
                    } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
}

TEST_F(AccelerationStructureTest, CreateMultiAABBBLAS)
{
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    // Build BLAS with multiple AABBs.
    ctx.BuildBLAS(blas,
                  { .type = ASGeometryType::AABBs,
                    .geometry = {
                        BLASGeometryDesc{
                            .aabbs = { AABB{ 0, 0, 0, 1, 1, 0.5f }, AABB{ 0, 0, 0.5f, 1, 1, 1 }, },
                            .flags = ASGeometry::Opaque,
                        },
                    } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
}

TEST_F(AccelerationStructureTest, CreateMultiAABBAndTLAS)
{
    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });
    auto tlas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_TLAS", .type = ASType::TopLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    // Build BLAS with multiple AABBs.
    ctx.BuildBLAS(blas,
                  { .type = ASGeometryType::AABBs,
                    .geometry = {
                        BLASGeometryDesc{
                            .aabbs = { AABB{ 0, 0, 0, 1, 1, 0.5f }, AABB{ 0, 0, 0.5f, 1, 1, 1 }, },
                            .flags = ASGeometry::Opaque,
                        },
                    } });
    TLASInstanceDesc instanceDesc{
        .blas = blas,
    };
    ctx.BuildTLAS(tlas, { .instances = { instanceDesc } });
    graphics.Submit(ctx);
    graphics.DestroyAccelerationStructure(blas);
    graphics.DestroyAccelerationStructure(tlas);
}

struct ASAABBTestData
{
    AABB aabb;
    float expectedResult;
    const char* testName;
};

struct ASAABBTest
    : public AccelerationStructureTest
    , public testing::WithParamInterface<ASAABBTestData>
{
};

TEST_P(ASAABBTest, CreateAABBTraceShader)
{
    const auto& testData = GetParam();

    auto blas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });
    auto tlas = graphics.CreateAccelerationStructure(
        ASDesc{ .name = "AABB_TLAS", .type = ASType::TopLevel, .buildFlags = ASBuild::None });

    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    // Build BLAS with multiple AABBs.
    ctx.BuildBLAS(blas,
                  { .type = ASGeometryType::AABBs,
                    .geometry = {
                        BLASGeometryDesc{
                            .aabbs = { testData.aabb },
                            .flags = ASGeometry::Opaque,
                        },
                    } });
    TLASInstanceDesc instanceDesc{ .blas = blas };
    ctx.BuildTLAS(tlas, { .instances = { instanceDesc } });

    ctx.Barrier(blas, RHIBarrierSync::AllCommands, RHIBarrierAccess::ShaderRead);
    ctx.Barrier(tlas, RHIBarrierSync::AllCommands, RHIBarrierAccess::ShaderRead);

    Buffer out = graphics.CreateBuffer(BufferDesc::CreateGenericBufferDesc("DataOut", 4, true));

    struct Data
    {
        BindlessHandle outputHandle;
        BindlessHandle tlasHandle;
    } data{
        .outputHandle = graphics.GetBindlessHandle(BufferBinding{
            .buffer = out,
            .usage = BufferBindingUsage::RWStructuredBuffer,
            .strideByteSize = 4,
        }),
        .tlasHandle = graphics.GetBindlessHandle(tlas),
    };

    const auto shaderPath = VexRootPath / "tests/shaders/RayTracingAABB.hlsl";

    ctx.TraceRays(
        RayTracingPassDesc{
            .rayGenerationShader = ShaderKey{
                 .path = shaderPath,
                 .entryPoint = "RayGenMain",
                 .type = ShaderType::RayGenerationShader,
             },
             .rayMissShaders =
             {
                 ShaderKey{
                     .path = shaderPath,
                     .entryPoint = "MissMain",
                     .type = ShaderType::RayMissShader,
                 }
             },
             .hitGroups =
             {
                 HitGroup{
                     .name = "Test_RayTracing_AABB_HitGroup",
                     .rayClosestHitShader = 
                     {
                         .path = shaderPath,
                         .entryPoint = "ClosestHitMain",
                         .type = ShaderType::RayClosestHitShader,
                     },
                     .rayIntersectionShader =
                     ShaderKey{
                          .path = shaderPath,
                          .entryPoint = "IntersectMain",
                          .type = ShaderType::RayIntersectionShader,
                     },
                 }
             },
             // Allow for primary rays only (no recursion).
             .maxRecursionDepth = 1,
             .maxPayloadByteSize = sizeof(float),
             .maxAttributeByteSize = sizeof(float) * 2,
        },
        ConstantBinding(data),
        {
            1, 1, 1
        }
    );

    ctx.BarrierBinding({ .buffer = out, .usage = BufferBindingUsage::RWStructuredBuffer });

    BufferReadbackContext readback = ctx.EnqueueDataReadback(out);

    graphics.WaitForTokenOnCPU(graphics.Submit(ctx));

    float f;
    readback.ReadData(std::as_writable_bytes(std::span<float>(&f, 1)));

    EXPECT_FLOAT_EQ(f, testData.expectedResult);

    graphics.DestroyAccelerationStructure(blas);
    graphics.DestroyBuffer(out);
}

INSTANTIATE_TEST_SUITE_P(ASAABBTestSuite,
                         ASAABBTest,
                         testing::Values(ASAABBTestData{ .aabb = AABB{ 0, 0, 1, 1, 1, 2 },
                                                         .expectedResult = 1,
                                                         .testName = "Has_Intersection_RayOriginOutsideAABB" },
                                         ASAABBTestData{ .aabb = AABB{ 0, 0, -1, 1, 1, 1 },
                                                         .expectedResult = 1,
                                                         .testName = "Has_Intersection_RayOriginInsideAABB" },
                                         ASAABBTestData{ .aabb = AABB{ 1, 1, 1, 2, 2, 2 },
                                                         .expectedResult = -1,
                                                         .testName = "No_Intersection" }),
                         [](const testing::TestParamInfo<ASAABBTestData>& info) { return info.param.testName; });

#endif
