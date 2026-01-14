#include "VexTest.h"

#include <gtest/gtest.h>

using namespace vex;

struct AccelerationStructureTest : VexTest
{
    using Vertex = std::array<float, 3>;

    Buffer triangleVertexBuffer;
    Buffer triangleIndexBuffer;

    void SetUp() override
    {
        auto ctx = graphics.CreateCommandContext(QueueType::Compute);

        const vex::BufferDesc vbDesc =
            vex::BufferDesc::CreateVertexBufferDesc("RT Triangle Vertex Buffer", sizeof(Vertex) * TriangleVerts.size());
        triangleVertexBuffer = graphics.CreateBuffer(vbDesc);

        const vex::BufferDesc ibDesc =
            vex::BufferDesc::CreateIndexBufferDesc("RT Triangle Index Buffer",
                                                   sizeof(vex::u32) * TriangleIndices.size());
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
    static constexpr std::array<vex::u32, 3> TriangleIndices{ 0, 1, 2 };
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
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<vex::u32>(sizeof(Vertex)), },
                        .transform = std::nullopt,
                        .flags = vex::ASGeometry::Opaque,
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
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = vex::ASGeometry::Opaque,
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
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = {{ 
                                1, 0, 0, 1,
                                0, 1, 0, 5,
                                0, 0, 1, -10,
                        }},
                        .flags = vex::ASGeometry::Opaque,
                    } } });
    ctx.BuildBLAS(blas2,
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = {{ 
                                1, 0, 0, 10,
                                0, 1, 0, -5,
                                0, 0, 1, 1,
                        }},
                        .flags = vex::ASGeometry::Opaque,
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
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
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
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = vex::ASGeometry::Opaque,
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

    vex::TLASInstanceDesc instanceDesc{
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
    std::array<vex::TLASInstanceDesc, 2> instances{
        vex::TLASInstanceDesc{
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
        vex::TLASInstanceDesc{
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
    vex::TLASInstanceDesc instanceDesc{
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
