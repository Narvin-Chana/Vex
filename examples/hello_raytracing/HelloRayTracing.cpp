#include <array>
// This include order has to be used to not have ODR violation when using the vex module, this is due to a  bug in the MSVC implementation.

#include "HelloRayTracing.h"

#include <span>

#include <GLFWIncludes.h>

HelloRayTracing::HelloRayTracing()
    : ExampleApplication("HelloRayTracing")
{
    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

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
        Vertex{ 0, Offset, DepthValue },
        Vertex{ Offset, -Offset, DepthValue },
        Vertex{ -Offset, -Offset, DepthValue },
    };
    static constexpr std::array<vex::u32, 3> TriangleIndices{ 0, 1, 2 };

    triangleBLAS = graphics->CreateAccelerationStructure({
        .name = "TriangleBLAS",
        .type = vex::ASType::BottomLevel,
        .buildFlags = vex::ASBuild::PreferFastTrace,
    });
    tlas = graphics->CreateAccelerationStructure({
        .name = "HelloRayTracing_TLAS",
        .type = vex::ASType::TopLevel,
        .buildFlags = vex::ASBuild::PreferFastTrace,
    });

    // Create vertex and index buffers
    const vex::BufferDesc vbDesc =
        vex::BufferDesc::CreateVertexBufferDesc("RT Vertex Buffer", sizeof(Vertex) * TriangleVerts.size(), false, true);
    vex::Buffer vertexBuffer = graphics->CreateBuffer(vbDesc);

    const vex::BufferDesc ibDesc = vex::BufferDesc::CreateIndexBufferDesc("RT Index Buffer",
                                                                          sizeof(vex::u32) * TriangleIndices.size(),
                                                                          false,
                                                                          true);
    vex::Buffer indexBuffer = graphics->CreateBuffer(ibDesc);

    vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

    ctx.EnqueueDataUpload(vertexBuffer, std::as_bytes(std::span(TriangleVerts)));
    ctx.EnqueueDataUpload(indexBuffer, std::as_bytes(std::span(TriangleIndices)));

    ctx.BuildBLAS(triangleBLAS,
                  { .geometry = { vex::BLASGeometryDesc{
                        .vertexBufferBinding = vex::BufferBinding::CreateStructuredBuffer(vertexBuffer, sizeof(Vertex)),
                        .indexBufferBinding = vex::BufferBinding::CreateStructuredBuffer(indexBuffer, sizeof(vex::u32)),
                        .transform = std::nullopt,
                        .flags = vex::ASGeometry::Opaque,
                    } } });

    std::array<vex::TLASInstanceDesc, 2> instances{
        // Left triangle (in front)
        vex::TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, -0.3f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            },
            .instanceID = 0,
            .blas = triangleBLAS,
        },
        // Right triangle (behind)
        vex::TLASInstanceDesc{
            .transform = {
                1.0f, 0.0f, 0.0f, 0.3f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f,
            },
            .instanceID = 1,
            .blas = triangleBLAS,
        },
    };
    ctx.BuildTLAS(tlas, { .instances = instances });

    graphics->Submit(ctx);

    graphics->DestroyBuffer(vertexBuffer);
    graphics->DestroyBuffer(indexBuffer);
}

// Creates a ray tracing collection, used to specify the various shaders and RT-related properties for RT shader
// invocations. Typically, applications will have one of these with all the possible shaders for each material type.
static vex::RayTracingShaderCollection CreateRTCollection(vex::ShaderCompiler& sc, const std::string& shaderPath)
{
    return sc.GetRayTracingShaderCollection(vex::RayTracingShaderKey{
        // Allow for primary rays only (no recursion).
        .maxRecursionDepth = 1,
        // We use a payload of 3 floats (so 12 bytes).
        .maxPayloadByteSize = 12,
        // We use the built-in hlsl attributes (so 8 bytes).
        .maxAttributeByteSize = 8,
        .rayGenerationShaders = 
        { 
            vex::ShaderKey{
                .filepath = shaderPath,
                .entryPoint = "RayGenMain",
                .type = vex::ShaderType::RayGenerationShader,
            },
         },
         .rayMissShaders =
         {
             vex::ShaderKey{
                 .filepath = shaderPath,
                 .entryPoint = "RayMiss",
                 .type = vex::ShaderType::RayMissShader,
             }
         },
         .hitGroups =
         {
             vex::HitGroupKey{
                 .name = "HelloRayTracing_HitGroup",
                 .rayClosestHitShader = 
                 {
                     .filepath = shaderPath,
                     .entryPoint = "RayClosestHit",
                     .type = vex::ShaderType::RayClosestHitShader,
                 },
             }
         },
    });
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
            static const vex::RayTracingShaderCollection HLSLRayTracingPassDesc =
                CreateRTCollection(shaderCompiler, HLSLShaderPath.string());
            ctx.TraceRays(HLSLRayTracingPassDesc,
                          vex::ConstantBinding(data),
                          { outputTextureBinding },
                          { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );

            static const std::filesystem::path SlangShaderPath =
                ExamplesDir / "hello_raytracing" / "HelloRayTracingShader.slang";
            static const vex::RayTracingShaderCollection SlangRayTracingPassDesc =
                CreateRTCollection(shaderCompiler, SlangShaderPath.string());
            ctx.TraceRays(SlangRayTracingPassDesc,
                          vex::ConstantBinding(data),
                          { outputTextureBinding },
                          { static_cast<vex::u32>(width), static_cast<vex::u32>(height), 1 } // One ray per pixel.
            );

            // Copy output to the backbuffer.
            ctx.Copy(workingTexture, graphics->GetCurrentPresentTexture());

            graphics->Submit(ctx);
        }

        graphics->Present();
    }
}

void HelloRayTracing::OnResize(GLFWwindow* window, int newWidth, int newHeight)
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
        .width = static_cast<uint32_t>(newWidth),
        .height = static_cast<uint32_t>(newHeight),
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