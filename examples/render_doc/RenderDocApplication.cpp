#include "RenderDocApplication.h"

#include <GLFWIncludes.h>
#include <VexRenderDoc.h>

RenderDocApplication::RenderDocApplication()
    : ExampleApplication("RenderDocApplication")
{
    // Graphics debuggers have to be initialized before graphics device creation.
    RenderDoc::Setup();

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    SetupShaderErrorHandling();
}

RenderDocApplication::~RenderDocApplication()
{
    RenderDoc::Teardown();
}

void RenderDocApplication::Run()
{
    static bool hasCaptured = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!hasCaptured)
        {
            RenderDoc::StartCapture();

            RenderDoc::EndCapture();
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

int main()
{
    RenderDocApplication application;
    application.Run();
}