#include "HelloRayTracing.h"

#include <array>
#include <vector>

#include <GLFWIncludes.h>

HelloRayTracing::HelloRayTracing()
    : ExampleApplication("HelloRayTracing")
{
    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    SetupShaderErrorHandling();

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .format = vex::TextureFormat::BGRA8_UNORM,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrSliceCount = 1,
                                  .mips = 1,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite });

    using Vertex = std::array<float, 3>;
    static constexpr float DepthValue = 1.0;
    static constexpr float Offset = 0.7f;
    static constexpr std::array TriangleVerts{
        // Triangle
        Vertex{ 0, -Offset, DepthValue },
        Vertex{ -Offset, Offset, DepthValue },
        Vertex{ Offset, Offset, DepthValue },
    };
    static constexpr std::array<vex::u32, 3> TriangleIndices{ 0, 1, 2 };

    triangleBLAS = graphics->CreateBottomLevelAccelerationStructure(
        { .name = "TriangleBLAS", .buildFlags = vex::ASBuildFlags::PreferFastTrace });
    tlas = graphics->CreateTopLevelAccelerationStructure({ .name = "HelloRayTracing_TLAS" });

    // Create vertex and index buffers
    const vex::BufferDesc vbDesc =
        vex::BufferDesc::CreateVertexBufferDesc("RT Vertex Buffer", sizeof(Vertex) * TriangleVerts.size());
    vex::Buffer vertexBuffer = graphics->CreateBuffer(vbDesc);

    const vex::BufferDesc ibDesc =
        vex::BufferDesc::CreateIndexBufferDesc("RT Index Buffer", sizeof(vex::u32) * TriangleIndices.size());
    vex::Buffer indexBuffer = graphics->CreateBuffer(ibDesc);

    vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

    ctx.EnqueueDataUpload(vertexBuffer, std::as_bytes(std::span(TriangleVerts)));
    ctx.EnqueueDataUpload(indexBuffer, std::as_bytes(std::span(TriangleIndices)));
    ctx.BuildBLAS(triangleBLAS,
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = { .buffer = vertexBuffer, .strideByteSize = static_cast<vex::u32>(sizeof(Vertex)), },
                        .indexBufferBinding = vex::BufferBinding{ .buffer = indexBuffer, .strideByteSize = static_cast<vex::u32>(sizeof(vex::u32)), },
                        .transform = std::nullopt,
                        .flags = vex::ASGeometryFlags::Opaque,
                    } } });

    ctx.Barrier(triangleBLAS, vex::RHIBarrierSync::AllCommands, vex::RHIBarrierAccess::AccelerationStructureRead);

    std::array<vex::TLASInstanceDesc, 2> instances{
        // Left triangle
        vex::TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, -1.5f,
                0.0f, 1.0f, 0.0f,  0.0f,
                0.0f, 0.0f, 1.0f,  0.0f,
            },
            .instanceID = 0,
            .blas = triangleBLAS,
        },
        // Right triangle
        vex::TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, 1.5f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            },
            .instanceID = 1,
            .blas = triangleBLAS,
        },
    };
    ctx.BuildTLAS(tlas, { .instances = instances });

    ctx.Barrier(tlas, vex::RHIBarrierSync::AllCommands, vex::RHIBarrierAccess::AccelerationStructureRead);

    graphics->Submit(ctx);

    graphics->DestroyBuffer(vertexBuffer);
    graphics->DestroyBuffer(indexBuffer);
}

void HelloRayTracing::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        {
            vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

            const vex::TextureBinding outputTextureBinding{
                .texture = workingTexture,
                .usage = vex::TextureBindingUsage::ShaderReadWrite,
            };

            // Make sure our resource is ready for writing.
            ctx.BarrierBinding(outputTextureBinding);

            struct Data
            {
                vex::BindlessHandle outputHandle;
                vex::BindlessHandle accelerationStructureHandle;
            } data{
                .outputHandle = graphics->GetBindlessHandle(outputTextureBinding),
                .accelerationStructureHandle = graphics->GetBindlessHandle(tlas),
            };

            // Two ray generation invocations, one for the HLSL shader, and one for the Slang shader.
            // The HLSL shader will write to the left side and the Slang shader to the right side.
            // Since we know the writes will not overlap, we don't have to add a barrier between the two.

            static const std::filesystem::path HLSLShaderPath =
                ExamplesDir / "hello_raytracing" / "HelloRayTracingShader.hlsl";

            ctx.TraceRays(
                vex::RayTracingPassDescription{ 
                    .rayGenerationShader =
                    vex::ShaderKey{
                        .path = HLSLShaderPath,
                        .entryPoint = "RayGenMain",
                        .type = vex::ShaderType::RayGenerationShader,
                    },
                    .rayMissShaders =
                    {
                        vex::ShaderKey{
                            .path = HLSLShaderPath,
                            .entryPoint = "RayMiss",
                            .type = vex::ShaderType::RayMissShader,
                        }
                    },
                    .hitGroups =
                    {
                        vex::HitGroup{
                            .name = "HelloRayTracing_HitGroup",
                            .rayClosestHitShader = 
                            {
                                .path = HLSLShaderPath,
                                .entryPoint = "RayClosestHit",
                                .type = vex::ShaderType::RayClosestHitShader,
                            },
                        }
                    },
                    // Allow for primary rays only.
                    .maxRecursionDepth = 1,
                    // We use a payload of 3 floats (so 12 bytes).
                    .maxPayloadByteSize = 12,
                    // We use the built-in hlsl attributes (so 8 bytes).
                    .maxAttributeByteSize = 8,
                },
                vex::ConstantBinding(data),
                { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );

// #if VEX_SLANG
#if 0
            ctx.TraceRays(
                { 
                    .rayGenerationShader = 
                    {
                        .path = ExamplesDir / "hello_raytracing" / "HelloRayTracingShader.slang",
                        .entryPoint = "RayGenMain",
                        .type = vex::ShaderType::RayGenerationShader,
                    },
                },
                vex::ConstantBinding(data),
                { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );
#endif

            // Copy output to the backbuffer.
            ctx.Copy(workingTexture, graphics->GetCurrentPresentTexture());

            graphics->Submit(ctx);
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void HelloRayTracing::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);

    ExampleApplication::OnResize(window, newWidth, newHeight);

    workingTexture = graphics->CreateTexture({
        .name = "Working Texture",
        .type = vex::TextureType::Texture2D,
        .format = vex::TextureFormat::BGRA8_UNORM,
        .width = newWidth,
        .height = newHeight,
        .depthOrSliceCount = 1,
        .mips = 1,
        .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite,
    });
}

int main()
{
    HelloRayTracing application;
    application.Run();
}