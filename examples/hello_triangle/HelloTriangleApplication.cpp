#include "HelloTriangleApplication.h"

#include <GLFW/glfw3.h>
#include <math.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#if defined(__linux__)
// Undefine/define problematic X11 macros
#ifdef Always
#undef Always
#endif
#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#ifndef Bool
#define Bool bool
#endif
#endif

#include <GLFW/glfw3native.h>

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

#if defined(_WIN32)
    // Suggestion of an intrusive (a la Unreal) way to display errors.
    // The handling of shader compilation errors is user choice.
    graphics->SetShaderCompilationErrorsCallback(
        [window = window](const std::vector<std::pair<vex::ShaderKey, std::string>>& errors) -> bool
        {
            if (!errors.empty())
            {
                std::string totalErrorMessage = "Error compiling shader(s):\n";
                for (auto& [key, err] : errors)
                {
                    totalErrorMessage.append(std::format("Shader: {} - Error: {}\n", key, err));
                }
                totalErrorMessage.append("\nDo you want to retry?");

                vex::i32 result = MessageBox(NULL,
                                             totalErrorMessage.c_str(),
                                             "Shader Compilation Error",
                                             MB_ICONERROR | MB_YESNO | MB_DEFBUTTON2);
                if (result == IDYES)
                {
                    return true;
                }
                else if (result == IDNO)
                {
                    VEX_LOG(vex::Error, "Unable to continue with shader errors. Closing application.");
                    glfwSetWindowShouldClose(window, true);
                }
            }

            return false;
        });
#endif
}

HelloTriangleApplication::~HelloTriangleApplication()
{
}

void HelloTriangleApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_R)
        {
            graphics->RecompileChangedShaders();
        }
    }
}

void HelloTriangleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const double currentTime = glfwGetTime();

        graphics->StartFrame();

        {
            float ocillatedColor = static_cast<float>(cos(currentTime) / 2 + 0.5);
            float invOcillatedColor = 1 - ocillatedColor;
            float color[4] = { invOcillatedColor, ocillatedColor, invOcillatedColor, 1.0 };
            graphics->UpdateData(colorBuffer, color);

            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);
            ctx.Dispatch(
                { .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
                          "examples" / "hello_triangle" / "HelloTriangleShader.cs.hlsl",
                  .entryPoint = "CSMain",
                  .type = vex::ShaderType::ComputeShader },
                { .reads = { { .name = "ColorBuffer",
                               .buffer = colorBuffer,
                               .bufferUsage = vex::BufferBindingUsage::ConstantBuffer } },
                  .writes = { { .name = "OutputTexture", .texture = workingTexture },
                              { .name = "CommBuffer",
                                .buffer = commBuffer,
                                .bufferUsage = vex::BufferBindingUsage::RWStructuredBuffer,
                                .bufferStride = sizeof(float) * 4 } },
                  .constants = {} },
                { static_cast<vex::u32>(width) / 8, static_cast<vex::u32>(height) / 8, 1 });
            ctx.Dispatch(
                { .path = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
                          "examples" / "hello_triangle" / "HelloTriangleShader2.cs.hlsl",
                  .entryPoint = "CSMain",
                  .type = vex::ShaderType::ComputeShader },
                {
                    .reads = { { .name = "CommBuffer",
                                 .buffer = commBuffer,
                                 .bufferUsage = vex::BufferBindingUsage::StructuredBuffer,
                                 .bufferStride = sizeof(float) * 4 },
                               { .name = "SourceTexture", .texture = workingTexture } },
                    .writes = { { .name = "OutputTexture", .texture = finalOutputTexture } },
                    .constants = {},
                },
                { static_cast<vex::u32>(width) / 8, static_cast<vex::u32>(height) / 8, 1 });
            ctx.Copy(finalOutputTexture, graphics->GetCurrentBackBuffer());
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
