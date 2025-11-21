#include "RenderDocApplication.h"

#include <GLFWIncludes.h>
#include <VexRenderDoc.h>

RenderDocApplication::RenderDocApplication()
    : ExampleApplication("RenderDocApplication")
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

    SetupRenderDoc();
}

void RenderDocApplication::Run()
{
    static bool hasCaptured = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!hasCaptured)
        {
            StartRenderDocCapture();

            EndRenderDocCapture();
            VEX_LOG(vex::Info, "Capture frame with RenderDoc");
            hasCaptured = true;
        }

        graphics->Present(windowMode == Fullscreen);
    }
}

void RenderDocApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}