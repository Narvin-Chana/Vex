#include "PixEventsApplication.h"

#include <GLFWIncludes.h>
#include <VexPixEvents.h>

PixEventsApplication::PixEventsApplication()
    : ExampleApplication("PixEventsApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif
    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = SwapchainFormat,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
    SetupShaderErrorHandling();

    SetupPixEvents();
}

void PixEventsApplication::Run()
{
    static bool hasCaptured = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!hasCaptured)
        {
            StartPixEventsCapture(vex::StringToWString("ExampleCapture.wpix"));

            EndPixEventsCapture();
            VEX_LOG(vex::Info, "Capture frame with PIX");
            hasCaptured = true;
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void PixEventsApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}