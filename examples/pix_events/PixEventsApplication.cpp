#include "PixEventsApplication.h"

#include <GLFWIncludes.h>
#include <VexPixEvents.h>

PixEventsApplication::PixEventsApplication()
    : ExampleApplication("PixEventsApplication")
{
    // Graphics debuggers have to be initialized before graphics device creation.
    PIX::Setup();

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
    SetupShaderErrorHandling();
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

            using namespace vex;
            auto texture = graphics->CreateTexture(
                TextureDesc::CreateTexture2DDesc("TestDepthStencil",
                                                 TextureFormat::D32_FLOAT,
                                                 10,
                                                 10,
                                                 1,
                                                 TextureUsage::DepthStencil | TextureUsage::ShaderRead,
                                                 TextureClearValue{
                                                     .flags = TextureClear::ClearDepth,
                                                     .depth = .54,
                                                 }));

            auto ctx = graphics->CreateCommandContext(QueueType::Graphics);
            ctx.ClearTexture({ .texture = texture });

            ctx.EnqueueDataReadback(texture, TextureRegion::SingleMip(0));

            graphics->WaitForTokenOnCPU(graphics->Submit(ctx));

            PIX::EndCapture();
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

int main()
{
    PixEventsApplication application;
    application.Run();
}