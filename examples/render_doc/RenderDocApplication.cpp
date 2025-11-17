#include "RenderDocApplication.h"

#include <GLFWIncludes.h>
#include <RenderDocRenderExtension.h>

RenderDocApplication::RenderDocApplication()
    : ExampleApplication("RenderDocApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif
    auto rdocExtension = vex::MakeUnique<RenderDocRenderExtension>(platformWindow);

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = SwapchainFormat,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    graphics->RegisterRenderExtension(std::move(rdocExtension));
    SetupShaderErrorHandling();
}

void RenderDocApplication::Run()
{
    static bool hasCaptured = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!hasCaptured)
        {
            RenderDocRenderExtension::StartCapture();
        }

        graphics->Present(windowMode == Fullscreen);

        if (!hasCaptured)
        {
            RenderDocRenderExtension::EndCapture();
            VEX_LOG(vex::Info, "Capture frame with RenderDoc");
            hasCaptured = true;
        }
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