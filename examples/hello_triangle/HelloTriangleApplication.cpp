#include "HelloTriangleApplication.h"

#include <GLFWIncludes.h>

#define STB_IMAGE_IMPLEMENTATION
#include <third_party/stb_image.h>

HelloTriangleApplication::HelloTriangleApplication()
    : ExampleApplication("HelloTriangleApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = CreateGraphicsBackend(vex::BackendDescription{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
        .frameBuffering = vex::FrameBuffering::Triple,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    SetupShaderErrorHandling();

    workingTexture =
        graphics->CreateTexture({ .name = "Working Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);
    finalOutputTexture =
        graphics->CreateTexture({ .name = "Final Output Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = DefaultWidth,
                                  .height = DefaultHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);

    // Example of CPU accessible buffer
    colorBuffer = graphics->CreateBuffer({ .name = "Color Buffer",
                                           .byteSize = sizeof(float) * 4,
                                           .usage = vex::BufferUsage::UniformBuffer,
                                           .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                         vex::ResourceLifetime::Static);

    // Example of GPU only buffer
    commBuffer = graphics->CreateBuffer({ .name = "Comm Buffer",
                                          .byteSize = sizeof(float) * 4,
                                          .usage = vex::BufferUsage::ReadWriteBuffer | vex::BufferUsage::GenericBuffer,
                                          .memoryLocality = vex::ResourceMemoryLocality::GPUOnly },
                                        vex::ResourceLifetime::Static);

    graphics->StartFrame();

    auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

    const std::filesystem::path uvImagePath =
                    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
                    "examples" / "uv-guide.png";
    int width, height, channels;
    void* imageData = stbi_load(uvImagePath.string().c_str(), &width, &height, &channels, 4);

    std::vector<vex::u8> fullImageData;
    std::copy_n(static_cast<vex::u8*>(imageData), width * height * channels, std::back_inserter(fullImageData));

    // Checker board pattern for mip 2
    for (int x = 0; x < width / 2; ++x)
    {
        for (int y = 0; y < height / 2; ++y)
        {
            bool evenX = (x / 20) % 2 == 0;
            bool evenY = (y / 20) % 2 == 0;

            fullImageData.push_back(evenX ^ evenY ? 0 : 0xFF);
            fullImageData.push_back(0x00);
            fullImageData.push_back(0x00);
            fullImageData.push_back(0xFF);
        }
    }

    uvGuideTexture = graphics->CreateTexture(
        { .name = "UV Guide",
          .type = vex::TextureType::Texture2D,
          .width = static_cast<vex::u32>(width),
          .height = static_cast<vex::u32>(height),
          .depthOrArraySize = 1,
          .mips = 2,
          .format = vex::TextureFormat::RGBA8_UNORM,
          .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
        vex::ResourceLifetime::Static);

    ctx.EnqueueDataUpload(
        uvGuideTexture,
        std::span<const vex::u8>{ fullImageData });

    stbi_image_free(imageData);

    graphics->EndFrame(windowMode == Fullscreen);
}

void HelloTriangleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const double currentTime = glfwGetTime();

        graphics->StartFrame();

        {
            float oscillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOscillatedColor = 1 - oscillatedColor;
            float color[4] = { invOscillatedColor, oscillatedColor, invOscillatedColor, 1.0 };

// Debug to test allocations
#define VEX_TEST_ALLOCS 0
#if VEX_TEST_ALLOCS
            {
                std::vector<vex::Texture> textures;
                for (int i = 0; i < 100; i++)
                {
                    textures.push_back(graphics->CreateTexture(
                        { .name = std::format("DebugAllocTexture_{}", i),
                          .type = vex::TextureType::Texture2D,
                          .width = DefaultWidth,
                          .height = DefaultHeight,
                          .depthOrArraySize = 1,
                          .mips = 1,
                          .format = vex::TextureFormat::RGBA8_UNORM,
                          .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                        vex::ResourceLifetime::Static));
                }

                for (int i = 0; i < 100; i++)
                {
                    graphics->DestroyTexture(textures[i]);
                }
            }
#endif // VEX_TEST_ALLOCS

            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            // clang-format off
            ctx.EnqueueDataUpload(colorBuffer, color);

            ctx.Dispatch(
                {
                    .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() / "examples" / "hello_triangle" / "HelloTriangleShader.cs.hlsl",
                    .entryPoint = "CSMain",
                    .type = vex::ShaderType::ComputeShader
                },
                std::initializer_list<vex::ResourceBinding>{
                    vex::BufferBinding{
                        .name = "ColorBuffer",
                        .buffer = colorBuffer,
                        .usage = vex::BufferBindingUsage::ConstantBuffer
                    },
                    vex::BufferBinding{
                        .name = "CommBuffer",
                        .buffer = commBuffer,
                        .usage = vex::BufferBindingUsage::RWStructuredBuffer,
                        .stride = sizeof(float) * 4
                    },
                    vex::TextureBinding{
                        .name = "OutputTexture",
                        .texture = workingTexture,
                        .usage = vex::TextureBindingUsage::ShaderReadWrite,
                    },
                    vex::TextureBinding{
                        .name = "UVTextureGuide",
                        .texture = uvGuideTexture,
                        .usage = vex::TextureBindingUsage::ShaderRead
                    },
                },
                {},
                {
                    static_cast<vex::u32>(std::ceil(static_cast<float>(width) / 8.0f)),
                    static_cast<vex::u32>(std::ceil(static_cast<float>(height) / 8.0f)),
                    1
                });
            ctx.Dispatch(
                {
                    .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() / "examples" / "hello_triangle" / "HelloTriangleShader2.cs.hlsl",
                    .entryPoint = "CSMain",
                    .type = vex::ShaderType::ComputeShader
                },
                std::initializer_list<vex::ResourceBinding>{
                    vex::TextureBinding{
                        .name = "OutputTexture",
                        .texture = finalOutputTexture,
                        .usage = vex::TextureBindingUsage::ShaderReadWrite,
                    },
                    vex::BufferBinding{
                        .name = "CommBuffer",
                        .buffer = commBuffer,
                        .usage = vex::BufferBindingUsage::StructuredBuffer,
                        .stride = sizeof(float) * 4
                    },
                    vex::TextureBinding{
                        .name = "SourceTexture",
                        .texture = workingTexture,
                        .usage = vex::TextureBindingUsage::ShaderRead,
                    },
                },
                {},
                {
                    static_cast<vex::u32>(std::ceil(static_cast<float>(width) / 8.0f)),
                    static_cast<vex::u32>(std::ceil(static_cast<float>(height) / 8.0f)),
                    1
                });
            ctx.Copy(finalOutputTexture, graphics->GetCurrentBackBuffer());

            // clang-format on
        }

        graphics->EndFrame(windowMode == Fullscreen);
    }
}

void HelloTriangleApplication::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);
    graphics->DestroyTexture(finalOutputTexture);

    ExampleApplication::OnResize(window, newWidth, newHeight);

    finalOutputTexture =
        graphics->CreateTexture({ .name = "Final Output Texture",
                                  .type = vex::TextureType::Texture2D,
                                  .width = newWidth,
                                  .height = newHeight,
                                  .depthOrArraySize = 1,
                                  .mips = 1,
                                  .format = vex::TextureFormat::RGBA8_UNORM,
                                  .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite },
                                vex::ResourceLifetime::Static);
    workingTexture = graphics->CreateTexture(
        {
            .name = "Working Texture",
            .type = vex::TextureType::Texture2D,
            .width = newWidth,
            .height = newHeight,
            .depthOrArraySize = 1,
            .mips = 1,
            .format = vex::TextureFormat::RGBA8_UNORM,
            .usage = vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite,
        },
        vex::ResourceLifetime::Static);
}
