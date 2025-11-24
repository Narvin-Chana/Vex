#include "ImGuiApplication.h"

#include <GLFWIncludes.h>
#include <ImGuiRenderExtension.h>

ImGuiApplication::ImGuiApplication()
    : ExampleApplication("ImGuiApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif
    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    graphics->RegisterRenderExtension(
        vex::MakeUnique<ImGuiRenderExtension>(*graphics, window, FrameBuffering, SwapchainFormat));
    SetupShaderErrorHandling();
}

void ImGuiApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->Present(windowMode == Fullscreen);
    }
}

void ImGuiApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}

int main()
{
    ImGuiApplication application;
    application.Run();
}