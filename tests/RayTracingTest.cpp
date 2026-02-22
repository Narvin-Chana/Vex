#include "VexTest.h"

#include <span>

#include <gtest/gtest.h>

using namespace vex;

struct RTTestFixture : public RTVexTest
{
    using Vertex = std::array<float, 3>;

    Buffer triangleVertexBuffer;
    Buffer triangleIndexBuffer;
    Buffer outputBuffer;
    AccelerationStructure triangleBLAS;
    AccelerationStructure triangleTLAS;

    struct ConstantData
    {
        BindlessHandle tlas;
        BindlessHandle output;
    } data;

    void SetUp() override
    {
        RTVexTest::SetUp();

        triangleBLAS = graphics.CreateAccelerationStructure(
            ASDesc{ .name = "Triangle BLAS", .type = ASType::BottomLevel, .buildFlags = ASBuild::None });
        triangleTLAS = graphics.CreateAccelerationStructure(
            ASDesc{ .name = "Triangle TLAS", .type = ASType::TopLevel, .buildFlags = ASBuild::None });

        auto ctx = graphics.CreateCommandContext(QueueType::Compute);
        const BufferDesc vbDesc =
            BufferDesc::CreateVertexBufferDesc("RT Triangle Vertex Buffer", sizeof(Vertex) * TriangleVerts.size());
        triangleVertexBuffer = graphics.CreateBuffer(vbDesc);

        const BufferDesc ibDesc =
            BufferDesc::CreateIndexBufferDesc("RT Triangle Index Buffer", sizeof(u32) * TriangleIndices.size());
        triangleIndexBuffer = graphics.CreateBuffer(ibDesc);

        const BufferDesc outputDesc = BufferDesc::CreateGenericBufferDesc("RT Output Buffer", sizeof(float) * 3, true);
        outputBuffer = graphics.CreateBuffer(outputDesc);

        ctx.EnqueueDataUpload(triangleVertexBuffer, std::as_bytes(std::span(TriangleVerts)));
        ctx.EnqueueDataUpload(triangleIndexBuffer, std::as_bytes(std::span(TriangleIndices)));

        ctx.BuildBLAS(triangleBLAS,
                  { .geometry = { BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = triangleVertexBuffer, .strideByteSize = static_cast<u32>(sizeof(Vertex)), },
                        .indexBufferBinding = BufferBinding{ .buffer = triangleIndexBuffer, .strideByteSize = static_cast<u32>(sizeof(u32)), },
                        .transform = std::nullopt,
                        .flags = ASGeometry::Opaque,
                    } } });

        TLASInstanceDesc instanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, -0.3f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            },
            .instanceID = 0,
            .blas = triangleBLAS,
        };
        ctx.BuildTLAS(triangleTLAS, { .instances = { instanceDesc } });

        ctx.BarrierBinding(BufferBinding{
            .buffer = outputBuffer,
            .usage = BufferBindingUsage::RWStructuredBuffer,
            .strideByteSize = static_cast<u32>(sizeof(float) * 3),
        });
        ctx.Barrier(triangleTLAS, RHIBarrierSync::AllCommands, RHIBarrierAccess::AccelerationStructureRead);
        data.tlas = graphics.GetBindlessHandle(triangleTLAS);
        data.output = graphics.GetBindlessHandle(BufferBinding{
            .buffer = outputBuffer,
            .usage = BufferBindingUsage::RWStructuredBuffer,
            .strideByteSize = static_cast<u32>(sizeof(float) * 3),
        });

        graphics.Submit(ctx);
    }

    void TearDown() override
    {
        graphics.DestroyAccelerationStructure(triangleTLAS);
        graphics.DestroyAccelerationStructure(triangleBLAS);
        graphics.DestroyBuffer(triangleVertexBuffer);
        graphics.DestroyBuffer(triangleIndexBuffer);
        graphics.DestroyBuffer(outputBuffer);
        RTVexTest::TearDown();
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

enum RTShaderType
{
    HLSL,
    SLANG,
};

const std::filesystem::path ShaderPath = VexRootPath / "tests/shaders";
static std::filesystem::path GetShaderName(RTShaderType shaderType)
{
    if (shaderType == HLSL)
    {
        return ShaderPath / "RayTracingTest.hlsl";
    }
    else // if (shaderType == SLANG)
    {
        return ShaderPath / "RayTracingTest.slang";
    }
}

struct RTShaderTest
    : public RTTestFixture
    , public testing::WithParamInterface<RTShaderType>
{
};

TEST_P(RTShaderTest, CompilePipeline_SingleRayGen_SingleMiss_SingleHitGroup)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    // Simple pipeline with one of each shader type (just the minimum needed ones).
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenBasicMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        {1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_MultipleRayGen)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    // Runs a RT collection with two raygens while selecting the second one.
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenBasicMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
ConstantBinding(data),
        { .width = 1, .height = 1, .depth = 1, .rayGenShaderIndex = 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_MultipleMissShaders)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenBasicMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissShadow",
                    .type = ShaderType::RayMissShader,
                },
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_MultipleHitGroups)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenBasicMain",
                    .type = ShaderType::RayGenerationShader,
                    .defines = { {"HIT_GROUP_OFFSET", "1"} },
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "HitGroup1",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                },
                HitGroup{
                    .name = "HitGroup2",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMainAlt",
                        .type = ShaderType::RayClosestHitShader,
                    },
                },
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_HitGroup_WithAnyHit)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenBasicMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "HitGroupWithAnyHit",
                    .rayClosestHitShader = vex::ShaderKey{
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                    .rayAnyHitShader = vex::ShaderKey{
                        .path = shaderPath,
                        .entryPoint = "AnyHitMain",
                        .type = ShaderType::RayAnyHitShader,
                    },
                },
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
       ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_WithCallableShaders)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .rayCallableShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "CallableMain",
                    .type = ShaderType::RayCallableShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "CallableMainAlt",
                    .type = ShaderType::RayCallableShader,
                },
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, SBT_SelectDifferentRayGenShaders)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    // First dispatch with rayGenOffset = 0
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMainAlt",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        TraceRaysDesc{
            .width = 1, .height = 1, .depth = 1,
            .rayGenShaderIndex = 0,
        }
    );

    // Second dispatch with rayGenOffset = 1
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMainAlt",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        TraceRaysDesc{
            .width = 1, .height = 1, .depth = 1,
            .rayGenShaderIndex = 1,
        }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, SBT_SelectDifferentMissShaders)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                },
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissShadow",
                    .type = ShaderType::RayMissShader,
                },
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        TraceRaysDesc{
            .width = 1, .height = 1, .depth = 1,
            .rayMissShaderIndex = 1,
        }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, SBT_SelectDifferentHitGroups)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "HitGroup1",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                },
                HitGroup{
                    .name = "HitGroup2",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMainAlt",
                        .type = ShaderType::RayClosestHitShader,
                    },
                },
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        TraceRaysDesc{
            .width = 1, .height = 1, .depth = 1,
            .hitGroupShaderIndex = 1,
        }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_VariousRecursionDepths)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    // Test depth = 1 (no recursion)
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
        ConstantBinding(data),
        { 1, 1, 1 }
    );

    // Test depth = 5 (moderate recursion)
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 16,
            .maxAttributeByteSize = 8,
        },
       ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

TEST_P(RTShaderTest, CompilePipeline_VariousPayloadSizes)
{
    auto ctx = graphics.CreateCommandContext(QueueType::Compute);
    auto shaderPath = GetShaderName(GetParam());

    // Small payload
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 4,
            .maxAttributeByteSize = 8,
        },
       ConstantBinding(data),
        { 1, 1, 1 }
    );

    // Large payload
    ctx.TraceRays(
        RayTracingCollection{
            .rayGenerationShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "RayGenMain",
                    .type = ShaderType::RayGenerationShader,
                },
            },
            .rayMissShaders = {
                ShaderKey{
                    .path = shaderPath,
                    .entryPoint = "MissMain",
                    .type = ShaderType::RayMissShader,
                }
            },
            .hitGroups = {
                HitGroup{
                    .name = "SimpleHitGroup",
                    .rayClosestHitShader = {
                        .path = shaderPath,
                        .entryPoint = "ClosestHitMain",
                        .type = ShaderType::RayClosestHitShader,
                    },
                }
            },
            .maxPayloadByteSize = 128,
            .maxAttributeByteSize = 8,
        },
       ConstantBinding(data),
        { 1, 1, 1 }
    );

    graphics.Submit(ctx);
}

INSTANTIATE_TEST_SUITE_P(RTShaderTypesTest,
                         RTShaderTest,
                         testing::Values(HLSL /*, SLANG*/),
                         [](const ::testing::TestParamInfo<RTShaderType>& info)
                         { return std::string(magic_enum::enum_name(info.param)); });
