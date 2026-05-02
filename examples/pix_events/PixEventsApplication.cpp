// Modules do not compile in this file, due to issues with MSVC compiler.
// Windows includes are the cause!
#undef VEX_MODULES
#define VEX_MODULES 0
#include "PixEventsApplication.h"

#include <GLFWIncludes.h>
#include <VexPixEvents.h>

PixEventsApplication::PixEventsApplication()
    : ExampleApplication("PixEventsApplication")
{
    // Graphics debuggers have to be initialized before graphics device creation.
    PIX::Setup();

    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
}

PixEventsApplication::~PixEventsApplication()
{
    PIX::Teardown();
}

void PixEventsApplication::Run()
{
    static bool hasCaptured = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!hasCaptured)
        {
            PIX::StartCapture(vex::StringToWString("ExampleCapture.wpix"));

            PIX::EndCapture();
            VEX_LOG(vex::Info, "Capture frame with PIX");
            hasCaptured = true;
        }

        graphics->Present();
    }
}

void PixEventsApplication::OnResize(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}

int main()
{
    PixEventsApplication application;
    application.Run();
}