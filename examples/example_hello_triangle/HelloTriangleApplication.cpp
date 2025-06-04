#include "HelloTriangleApplication.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
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

#define FORCE_VULKAN 0

    graphics = CreateGraphicsBackend(
#if VEX_VULKAN and FORCE_VULKAN
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
}

HelloTriangleApplication::~HelloTriangleApplication()
{
}

void HelloTriangleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->StartFrame();

        {
            auto ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);
            ctx.Dispatch(
                { .path = "RANDOM_PATH_BLA_BLA_FOR NOW THIS ISN'T USED ANYWAYS\nTODO: IMPLEMENT SHADER COMPILATION",
                  .entryPoint = "CSMain",
                  .type = vex::ShaderType::ComputeShader },
                {},
                {},
                { { vex::ResourceBinding{ .name = "OutputTexture", .texture = workingTexture } } },
                { static_cast<uint32_t>(width) / 8, static_cast<uint32_t>(height) / 8, 1 });
            ctx.Copy(workingTexture, graphics->GetCurrentBackBuffer());
        }

        graphics->EndFrame();
    }
}

void HelloTriangleApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    // TODO: destroy working texture

    ExampleApplication::OnResize(window, width, height);

    // TODO: create working texture
}
