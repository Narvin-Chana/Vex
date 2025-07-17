#include "HelloTriangleGraphicsApplication.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#include <Vex/Logger.h>

HelloTriangleGraphicsApplication::HelloTriangleGraphicsApplication()
    : ExampleApplication("HelloTriangleGraphicsApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

#define USE_VULKAN 0

    graphics = CreateGraphicsBackend(
#if VEX_VULKAN and USE_VULKAN
        vex::GraphicsAPI::Vulkan,
#else // VEX_DX12 and not FORCE_VULKAN
        vex::GraphicsAPI::DirectX12,
#endif
        vex::BackendDescription{
            .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
            .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
            .enableGPUDebugLayer = !VEX_SHIPPING,
            .enableGPUBasedValidation = !VEX_SHIPPING });

    workingTexture = graphics->CreateTexture({ .name = "Working Texture",
                                               .type = vex::TextureType::Texture2D,
                                               .width = DefaultWidth,
                                               .height = DefaultHeight,
                                               .depthOrArraySize = 1,
                                               .mips = 1,
                                               .format = vex::TextureFormat::RGBA8_UNORM,
                                               .usage = vex::ResourceUsage::Read | vex::ResourceUsage::UnorderedAccess,
                                               .clearValue{ .enabled = false } },
                                             vex::ResourceLifetime::Static);

#if defined(_WIN32)
    // Suggestion of an intrusive (à la Unreal) way to display errors.
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

HelloTriangleGraphicsApplication::~HelloTriangleGraphicsApplication()
{
}

void HelloTriangleGraphicsApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_R)
        {
            graphics->RecompileChangedShaders();
        }
    }
}

void HelloTriangleGraphicsApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->StartFrame();

        {
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);
            ctx.Draw(
                { .vertexShader = { .path = std::filesystem::current_path()
                                                .parent_path()
                                                .parent_path()
                                                .parent_path()
                                                .parent_path() /
                                            "examples" / "example_hello_triangle_graphics_pipeline" /
                                            "HelloTriangleGraphicsShader.hlsl",
                                    .entryPoint = "VSMain",
                                    .type = vex::ShaderType::VertexShader },
                  .pixelShader = { .path = std::filesystem::current_path()
                                               .parent_path()
                                               .parent_path()
                                               .parent_path()
                                               .parent_path() /
                                           "examples" / "example_hello_triangle_graphics_pipeline" /
                                           "HelloTriangleGraphicsShader.hlsl",
                                   .entryPoint = "PSMain",
                                   .type = vex::ShaderType::PixelShader } },
                {},
                {},
                {},
                { { vex::ResourceBinding{ .name = "OutputTexture", .texture = graphics->GetCurrentBackBuffer() } } },
                3);
            // ctx.Copy(workingTexture, graphics->GetCurrentBackBuffer());
        }

        graphics->EndFrame(windowMode == Fullscreen);
    }
}

void HelloTriangleGraphicsApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    graphics->DestroyTexture(workingTexture);

    ExampleApplication::OnResize(window, width, height);

    workingTexture = graphics->CreateTexture({ .name = "Working Texture",
                                               .type = vex::TextureType::Texture2D,
                                               .width = width,
                                               .height = height,
                                               .depthOrArraySize = 1,
                                               .mips = 1,
                                               .format = vex::TextureFormat::RGBA8_UNORM,
                                               .usage = vex::ResourceUsage::Read | vex::ResourceUsage::UnorderedAccess,
                                               .clearValue{ .enabled = false } },
                                             vex::ResourceLifetime::Static);
}
